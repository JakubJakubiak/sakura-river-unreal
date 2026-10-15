# Wspólne narzędzia generatorów modeli Sakura River HD (Blender 5.x, uruchamiane bez okna: blender -b -P ...).
#
# Układ współrzędnych: plan sceny jest ten sam co w wersji przeglądarkowej (three.js, metry):
#   web x — w prawo, web y — w górę, web z — w stronę kamery (kamera patrzy w -z, w górę rzeki).
# W Unrealu: UE X = -web z (w głąb kadru), UE Y = web x (w prawo), UE Z = web y (w górę), centymetry.
# Import FBX do UE zamienia Blender (x, y, z) na UE (x, -y, z), więc w Blenderze stawiamy punkt web (x, y, z) w (-z, -x, y).
import bpy, math, os
import numpy as np

OUT = os.environ.get("SAKURA_OUT") or os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "out")
os.makedirs(OUT, exist_ok=True)
TAU = math.pi * 2


def web_to_bl(x, y, z):
    return (-z, -x, y)


# ---------- funkcje terenu (te same wzory co w index.html, wektorowo w numpy) ----------
def smooth(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def hash2(x, y):
    s = np.sin(x * 127.1 + y * 311.7) * 43758.5453
    return s - np.floor(s)


def noise(x, y):
    xi, yi = np.floor(x), np.floor(y)
    xf, yf = x - xi, y - yi
    u, v = xf * xf * (3 - 2 * xf), yf * yf * (3 - 2 * yf)
    a, b, c, d = hash2(xi, yi), hash2(xi + 1, yi), hash2(xi, yi + 1), hash2(xi + 1, yi + 1)
    return a + (b - a) * u + (c - a) * v + (a - b - c + d) * u * v


def fbm(x, y, oct=5, lac=2.03, gain=0.5):
    s, a, f, n = 0.0, 0.5, 1.0, 0.0
    for _ in range(oct):
        s = s + a * noise(x * f, y * f); n += a; a *= gain; f *= lac
    return s / n


def ridged(x, y, oct=5):
    s, a, f, n = 0.0, 0.5, 1.0, 0.0
    for _ in range(oct):
        r = 1.0 - np.abs(noise(x * f, y * f) * 2 - 1); s = s + a * r * r; n += a; a *= 0.5; f *= 2.07
    return s / n


def river_cx(z):
    z = np.asarray(z, dtype=np.float64)
    return np.where(z < -60, (z + 60) ** 2 * 0.012, np.where(z > 36, -((z - 36) ** 2) * 0.03, 0.0))


def river_half(z):
    return 12 + 2.2 * np.sin(z * 0.045) + 1.2 * np.sin(z * 0.11 + 1)


def river_dist(x, z):
    """Odległość od brzegu rzeki (ujemna w wodzie)."""
    return np.abs(x - river_cx(z)) - river_half(z)


def ground_h(x, z, hd=True):
    """Wysokość terenu w metrach. hd=True dokłada drobny relief, którego wersja web nie miała."""
    d = river_dist(x, z)
    water = -2.3 * np.minimum(1, -d / 4.5) - 0.25 * noise(x * 0.25, z * 0.25) * smooth(0, 3, -d)
    land = (0.9 * smooth(0, 4, d) + noise(x * 0.06 + 10, z * 0.06 + 10) * 3.2 * smooth(5, 45, d)
            + noise(x * 0.2, z * 0.2) * 0.35 * smooth(1, 6, d) + smooth(35, 140, d) * 16)
    if hd:
        land = land + (fbm(x * 0.7, z * 0.7, 3) - 0.5) * 0.22 * smooth(0.5, 3, d)        # drobne nierówności łąki
        water = water + (noise(x * 1.3, z * 0.6) - 0.5) * 0.12                          # zmarszczki dna
    return np.where(d < 0, water, land)


def mountains(x, z):
    """Dodatek gór w dalekim terenie (metry): pasma rosną dopiero 2–6 km od środka sceny, więc nad nimi zostaje niebo i Fuji."""
    dc = np.sqrt(x * x + (z + 60.0) ** 2)
    d = river_dist(x, z)
    r = ridged(x / 1400.0 + 3.1, z / 1400.0 - 7.7, 6)
    valley = smooth(40, 400, d)
    big = smooth(1800, 6500, dc) * (120 + 520 * r)
    hills = smooth(250, 1600, dc) * fbm(x / 300.0, z / 300.0, 4) * 40
    return (big + hills) * valley


# ---------- budowa siatek z tablic ----------
def srgb_to_lin(c):
    c = np.asarray(c, dtype=np.float64)
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def hexcol(h):
    return np.array([(h >> 16) & 255, (h >> 8) & 255, h & 255], dtype=np.float64) / 255.0


def make_mesh(name, verts, tris, mat_names=("default",), tri_mat=None, uv=None, col=None, smooth_shade=True):
    """verts (N,3) w układzie Blendera, tris (M,3) indeksy; uv (N,2) i col (N,4, liniowe) na wierzchołek."""
    verts = np.ascontiguousarray(verts, dtype=np.float32)
    tris = np.ascontiguousarray(tris, dtype=np.int32)
    me = bpy.data.meshes.new(name)
    me.vertices.add(len(verts))
    me.vertices.foreach_set("co", verts.ravel())
    me.loops.add(tris.size)
    me.loops.foreach_set("vertex_index", tris.ravel())
    me.polygons.add(len(tris))
    me.polygons.foreach_set("loop_start", np.arange(0, tris.size, 3, dtype=np.int32))
    if tri_mat is not None:
        me.polygons.foreach_set("material_index", np.ascontiguousarray(tri_mat, dtype=np.int32))
    if uv is not None:
        uvl = me.uv_layers.new(name="UVMap")
        uvl.data.foreach_set("uv", np.ascontiguousarray(np.asarray(uv, dtype=np.float32)[tris.ravel()]).ravel())
    if col is not None:
        ca = me.color_attributes.new(name="Col", type="FLOAT_COLOR", domain="POINT")
        ca.data.foreach_set("color", np.ascontiguousarray(col, dtype=np.float32).ravel())
        me.color_attributes.active_color = ca
    me.update(calc_edges=True)
    me.validate(clean_customdata=False)
    if smooth_shade:
        me.polygons.foreach_set("use_smooth", np.ones(len(me.polygons), dtype=bool))
    for m in mat_names:
        mat = bpy.data.materials.get(m) or bpy.data.materials.new(m)
        me.materials.append(mat)
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


def grid_tris(nx, nz):
    """Trójkąty siatki nx × nz wierzchołków (wiersze po x)."""
    i = np.arange(nx - 1); j = np.arange(nz - 1)
    I, J = np.meshgrid(i, j, indexing="xy")
    a = (J * nx + I).ravel(); b = a + 1; c = a + nx; d = c + 1
    return np.concatenate([np.stack([a, c, b], 1), np.stack([b, c, d], 1)])


def fix_up_normals(ob):
    """Siatka terenu ma patrzeć w górę."""
    me = ob.data
    n = np.zeros(len(me.polygons) * 3, dtype=np.float32)
    me.polygons.foreach_get("normal", n)
    if n[2::3].sum() < 0:
        me.flip_normals(); me.update()


def clear_scene():
    for o in list(bpy.data.objects): bpy.data.objects.remove(o, do_unlink=True)
    for m in list(bpy.data.meshes): bpy.data.meshes.remove(m)
    for m in list(bpy.data.materials): bpy.data.materials.remove(m)


def export_fbx(objs, name):
    path = os.path.join(OUT, name + ".fbx")
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs: o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_scale_options="FBX_SCALE_UNITS",
                             mesh_smooth_type="FACE", use_mesh_modifiers=True, add_leaf_bones=False,
                             colors_type="LINEAR", use_tspace=False, bake_anim=False, object_types={"MESH"})
    tris = sum(sum(len(p.vertices) - 2 for p in o.data.polygons) for o in objs)
    print(f"[sakura] {name}.fbx  {tris:,} trójkątów")
    return path


def join(objs, name):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs: o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    with bpy.context.temp_override(active_object=objs[0], selected_editable_objects=objs, selected_objects=objs):
        bpy.ops.object.join()
    ob = objs[0]; ob.name = name; ob.data.name = name
    return ob
