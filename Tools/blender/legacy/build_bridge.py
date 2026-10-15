# Łukowy most (taiko-bashi) z papierowymi lampionami oraz kamienna latarnia (tōrō) do sceny Sakura River.
# Budowane skryptem w Blenderze (bmesh). Most: oś X wzdłuż przęsła, środek w (0,0,0), woda na Z=0. Latarnia: podstawa w (0,0,0).
import bpy, bmesh, math
from mathutils import Matrix, Vector

COL = {"red": 0xc8321e, "redDark": 0x8f2014, "deck": 0x6e4a33, "stone": 0x8d8f8a, "black": 0x1b1b1f,
       "bronze": 0x8a6a2e, "paper": 0xff7a4a, "glow": 0xffc878}
SETS = {"Bridge": {k: bmesh.new() for k in COL}, "Lantern": {k: bmesh.new() for k in COL}}
bms = SETS["Bridge"]

def T(x, y, z): return Matrix.Translation((x, y, z))
def S(sx, sy, sz): return Matrix.Diagonal((sx, sy, sz, 1.0))
RY = lambda a: Matrix.Rotation(a, 4, "Y")
RZ = lambda a: Matrix.Rotation(a, 4, "Z")
RX = lambda a: Matrix.Rotation(a, 4, "X")

def box(mat, sx, sy, sz, x, y, z, rot=None):
    bmesh.ops.create_cube(bms[mat], size=1.0, matrix=T(x, y, z) @ (rot or Matrix.Identity(4)) @ S(sx, sy, sz))
def cyl(mat, r1, r2, h, x, y, z, seg=16, rot=None):
    bmesh.ops.create_cone(bms[mat], cap_ends=True, cap_tris=False, segments=seg, radius1=r1, radius2=r2, depth=h, matrix=T(x, y, z) @ (rot or Matrix.Identity(4)))
def sphere(mat, r, x, y, z, sx=1.0, sy=1.0, sz=1.0, u=12, v=8):
    bmesh.ops.create_uvsphere(bms[mat], u_segments=u, v_segments=v, radius=r, matrix=T(x, y, z) @ S(sx, sy, sz))
def torus(mat, R, r, x, y, z, seg=12, tube=4):
    bm = bms[mat]; rings = []
    for i in range(seg):
        a = 2 * math.pi * i / seg
        rings.append([bm.verts.new((x + (R + r * math.cos(2 * math.pi * j / tube)) * math.cos(a), y + (R + r * math.cos(2 * math.pi * j / tube)) * math.sin(a),
                                    z + r * math.sin(2 * math.pi * j / tube))) for j in range(tube)])
    for i in range(seg):
        A, B = rings[i], rings[(i + 1) % seg]
        for j in range(tube): bm.faces.new((A[j], B[j], B[(j + 1) % tube], A[(j + 1) % tube]))
def tube(mat, pts, r, seg=8):
    bm = bms[mat]; rings = []; n = len(pts); up = Vector((0, 0, 1)); P = [Vector(p) for p in pts]
    for i in range(n):
        t = (P[min(i + 1, n - 1)] - P[max(i - 1, 0)]).normalized()
        side = t.cross(up); side = side.normalized() if side.length > 1e-6 else Vector((1, 0, 0))
        n2 = side.cross(t).normalized()
        rings.append([bm.verts.new(P[i] + r * (math.cos(2 * math.pi * j / seg) * side + math.sin(2 * math.pi * j / seg) * n2)) for j in range(seg)])
    for i in range(n - 1):
        A, B = rings[i], rings[i + 1]
        for j in range(seg): bm.faces.new((A[j], B[j], B[(j + 1) % seg], A[(j + 1) % seg]))
    bm.faces.new(rings[0]); bm.faces.new(list(reversed(rings[-1])))
def sweep(mat, pts, half_w, half_h, y):
    """Belka o przekroju prostokątnym wzdłuż łamanej w płaszczyźnie XZ (szerokość wzdłuż Y)."""
    bm = bms[mat]; rings = []; n = len(pts)
    for i in range(n):
        x0, z0 = pts[max(i - 1, 0)]; x1, z1 = pts[min(i + 1, n - 1)]
        tx, tz = x1 - x0, z1 - z0; l = math.hypot(tx, tz); tx /= l; tz /= l
        x, z = pts[i]
        rings.append([bm.verts.new((x - tz * a * half_h, y + b * half_w, z + tx * a * half_h)) for (a, b) in ((-1, -1), (-1, 1), (1, 1), (1, -1))])
    for i in range(n - 1):
        A, B = rings[i], rings[i + 1]
        for j in range(4): bm.faces.new((A[j], A[(j + 1) % 4], B[(j + 1) % 4], B[j]))
    bm.faces.new(list(reversed(rings[0]))); bm.faces.new(rings[-1])
def giboshi(x, y, z, s=1.0):                  # cebulasta głowica słupka
    cyl("bronze", 0.12 * s, 0.1 * s, 0.09 * s, x, y, z + 0.045 * s, seg=10)
    sphere("bronze", 0.17 * s, x, y, z + 0.24 * s, sz=1.15, u=12, v=8)
    cyl("bronze", 0.07 * s, 0.005, 0.18 * s, x, y, z + 0.5 * s, seg=8)

# ================= MOST =================
L, H, Y0, WD = 13.0, 4.0, 0.9, 1.7
arc = lambda x: Y0 + H * (1 - (x / L) ** 2)
slope = lambda x: math.atan(-2 * H * x / (L * L))
NP = 48
line = lambda off=0.0, a=-L, b=L, n=NP: [(a + (b - a) * i / n, arc(a + (b - a) * i / n) + off) for i in range(n + 1)]

# pokład: deski w poprzek, dźwigary po bokach i podłużnica pod spodem
NPL = 64
for i in range(NPL):
    x = -L + (i + 0.5) * 2 * L / NPL
    box("deck", 2 * L / NPL * 0.9 / math.cos(slope(x)), 2 * WD + 0.1, 0.09, x, 0, arc(x) - 0.045, RY(-slope(x)))
for s in (-1, 1):
    sweep("redDark", line(-0.26), 0.11, 0.2, s * (WD + 0.02))
    sweep("red", line(-0.02), 0.13, 0.05, s * (WD + 0.02))
sweep("redDark", line(-0.3), 0.14, 0.14, 0.0)

# balustrady: słupki, trzy poręcze, głowice na słupkach głównych
NPOST = 20
for s in (-1, 1):
    y = s * WD
    for k in range(NPOST + 1):
        x = -L + k * 2 * L / NPOST; main = k % 5 == 0
        w, hgt = (0.3, 1.55) if main else (0.17, 1.22)
        box("red", w, w, hgt, x, y, arc(x) + hgt / 2 - 0.05)
        if main: giboshi(x, y, arc(x) + hgt - 0.05, 1.0)
        else:    box("black", w + 0.04, w + 0.04, 0.05, x, y, arc(x) + hgt - 0.03)
    tube("red", [(x, y, z) for (x, z) in line(1.08)], 0.085, seg=8)
    sweep("red", line(0.68), 0.045, 0.045, y)
    sweep("red", line(0.28), 0.045, 0.045, y)

# podpory: pary słupów z poprzeczkami
for x in (-8.5, -3.0, 3.0, 8.5):
    top = arc(x) - 0.4
    for s in (-1, 1): cyl("redDark", 0.3, 0.27, top + 0.7, x, s * 1.4, (top - 0.7) / 2, seg=14)
    box("redDark", 0.42, 2 * WD + 0.5, 0.3, x, 0, top + 0.02)
    box("redDark", 0.22, 3.3, 0.22, x, 0, top - 0.95)
    if top > 2.4: box("redDark", 0.22, 3.3, 0.22, x, 0, 0.75)
    for s in (-1, 1): box("black", 0.7, 0.7, 0.07, x, s * 1.4, top - 0.16)

# przyczółki
for s in (-1, 1): box("stone", 1.6, 2 * WD + 1.2, 1.8, s * (L + 0.55), 0, 0.15)

# papierowe lampiony (chōchin) na wysięgnikach po zewnętrznej stronie
for s in (-1, 1):
    for x in (-9.0, -4.5, 0.0, 4.5, 9.0):
        zt = arc(x) + 2.55; yo = s * (WD + 0.55)
        box("black", 0.1, 0.1, 2.6, x, s * WD, arc(x) + 1.3)
        box("black", 0.08, 0.66, 0.08, x, s * (WD + 0.3), zt)
        cyl("black", 0.012, 0.012, 0.2, x, yo, zt - 0.14, seg=4)
        zc = zt - 0.68
        sphere("paper", 0.3, x, yo, zc, sz=1.35, u=12, v=8)
        cyl("black", 0.15, 0.17, 0.07, x, yo, zc + 0.4, seg=10); cyl("black", 0.17, 0.15, 0.07, x, yo, zc - 0.4, seg=10)
        for q in (-0.2, 0.0, 0.2): torus("black", 0.3 * math.sqrt(1 - (q / 0.405) ** 2) + 0.004, 0.012, x, yo, zc + q, seg=12, tube=3)
        cyl("bronze", 0.03, 0.0, 0.16, x, yo, zc - 0.52, seg=6)

# ================= KAMIENNA LATARNIA =================
bms = SETS["Lantern"]
HEX = 6
cyl("stone", 0.6, 0.54, 0.18, 0, 0, 0.09, seg=HEX); cyl("stone", 0.46, 0.4, 0.14, 0, 0, 0.25, seg=HEX)          # podstawa
cyl("stone", 0.17, 0.14, 1.25, 0, 0, 0.94, seg=12); torus("stone", 0.165, 0.035, 0, 0, 0.95, seg=12, tube=4)     # trzon z pierścieniem
cyl("stone", 0.24, 0.47, 0.2, 0, 0, 1.66, seg=HEX); cyl("stone", 0.5, 0.5, 0.08, 0, 0, 1.8, seg=HEX)            # platforma
sphere("glow", 0.26, 0, 0, 2.1, sz=0.85, u=10, v=6)                                                             # ogień
for k in range(HEX):                                                                                           # komora ognia: słupki z prześwitami
    a = k * math.pi / 3 + math.pi / 6
    box("stone", 0.09, 0.09, 0.52, 0.34 * math.cos(a), 0.34 * math.sin(a), 2.1, RZ(a))
    a2 = k * math.pi / 3
    box("stone", 0.05, 0.34, 0.1, 0.3 * math.cos(a2), 0.3 * math.sin(a2), 1.9, RZ(a2)); box("stone", 0.05, 0.34, 0.1, 0.3 * math.cos(a2), 0.3 * math.sin(a2), 2.31, RZ(a2))
cyl("stone", 0.42, 0.42, 0.06, 0, 0, 2.39, seg=HEX)
cyl("stone", 0.86, 0.14, 0.46, 0, 0, 2.65, seg=HEX); cyl("stone", 0.9, 0.86, 0.06, 0, 0, 2.43, seg=HEX)         # dach
for k in range(HEX):                                                                                           # zawinięte naroża
    a = k * math.pi / 3 + math.pi / 6
    sphere("stone", 0.09, 0.84 * math.cos(a), 0.84 * math.sin(a), 2.5, u=8, v=6)
cyl("stone", 0.1, 0.13, 0.1, 0, 0, 2.92, seg=8); sphere("stone", 0.13, 0, 0, 3.07, sz=1.2, u=10, v=8)            # zwieńczenie

# ================= obiekty =================
def srgb(c): c /= 255.0; return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
LOC = {"Bridge": (0.0, -26.0, 0.0), "Lantern": (16.0, -14.0, 0.0)}     # tylko ustawienie w pliku .blend; dane siatki zostają w układzie modelu
result = {}
for set_name, group in SETS.items():
    for o in [o for o in bpy.data.objects if o.name.startswith(set_name + "_")]: bpy.data.objects.remove(o, do_unlink=True)
    coll = bpy.data.collections.get(set_name) or bpy.data.collections.new(set_name)
    if coll.name not in bpy.context.scene.collection.children: bpy.context.scene.collection.children.link(coll)
    tris = {}
    for name, hexc in COL.items():
        bm = group[name]
        if not bm.faces: bm.free(); continue
        mesh = bpy.data.meshes.new(set_name + "_" + name); bm.to_mesh(mesh); bm.free()
        mesh.polygons.foreach_set("use_smooth", [True] * len(mesh.polygons)); mesh.set_sharp_from_angle(angle=math.radians(35)); mesh.update()
        mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
        rgb = (srgb((hexc >> 16) & 255), srgb((hexc >> 8) & 255), srgb(hexc & 255), 1.0)
        mat.diffuse_color = rgb; mat.use_nodes = True
        bsdf = mat.node_tree.nodes.get("Principled BSDF")
        if bsdf:
            bsdf.inputs["Base Color"].default_value = rgb; bsdf.inputs["Roughness"].default_value = 0.8
            if name in ("glow", "paper"): bsdf.inputs["Emission Color"].default_value = rgb; bsdf.inputs["Emission Strength"].default_value = 2.0
        mesh.materials.append(mat)
        ob = bpy.data.objects.new(set_name + "_" + name, mesh); ob.location = LOC[set_name]; coll.objects.link(ob)
        tris[name] = sum(len(p.vertices) - 2 for p in mesh.polygons)
    result[set_name] = {"triangles": tris, "total": sum(tris.values())}
bpy.context.view_layer.update()
