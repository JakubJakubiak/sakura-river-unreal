# Budowle i drobne modele Sakura River HD:
#   SM_Pagoda, SM_Bridge, SM_StoneLantern — z wersji przeglądarkowej (legacy/build_*.py, te same proporcje), przeniesione do układu UE;
#   SM_Torii — brama w wodzie; SM_FloatLantern — tōrō nagashi (lampion z czatu); SM_Rock_1..4 — kamienie (Nanite);
#   SM_Petal, SM_RainStreak, SM_Spark — drobne siatki do efektów instancjonowanych na GPU.
import sys, os, math, random
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy, bmesh
import numpy as np
from mathutils import Matrix, Vector
from common import *

HERE = os.path.dirname(os.path.abspath(__file__))


def legacy_to_ue_axes(obs):
    """Modele legacy mają oś X wzdłuż web x i front w -Y (jak w glTF dla three.js). Obrót o -90° wokół Z
    daje układ wspólny dla wszystkich eksportów (Blender (-web z, -web x, web y))."""
    R = Matrix.Rotation(-math.pi / 2, 4, "Z")
    for o in obs:
        o.location = (0, 0, 0)
        o.data.transform(R)
        o.data.update()


def run_legacy(script, prefixes):
    clear_scene()
    g = {"__name__": "legacy", "OUT": OUT}
    exec(compile(open(os.path.join(HERE, "legacy", script)).read(), script, "exec"), g)
    out = {}
    for p in prefixes:
        obs = [o for o in bpy.data.objects if o.name.startswith(p + "_") and o.type == "MESH"]
        legacy_to_ue_axes(obs)
        out[p] = obs
    return out


# ---------- pomocnicze bmesh ----------
def T(x, y, z): return Matrix.Translation((x, y, z))
def S(sx, sy, sz): return Matrix.Diagonal((sx, sy, sz, 1.0))
RX = lambda a: Matrix.Rotation(a, 4, "X")
RY = lambda a: Matrix.Rotation(a, 4, "Y")
RZ = lambda a: Matrix.Rotation(a, 4, "Z")


class Builder:
    def __init__(self, mats):
        self.bms = {m: bmesh.new() for m in mats}

    def box(self, m, sx, sy, sz, x, y, z, rot=None, bevel=0.0):
        bm = bmesh.new()
        bmesh.ops.create_cube(bm, size=1.0, matrix=T(x, y, z) @ (rot or Matrix.Identity(4)) @ S(sx, sy, sz))
        if bevel > 0:
            bmesh.ops.bevel(bm, geom=list(bm.edges) + list(bm.verts), offset=bevel, segments=2, affect="EDGES", profile=0.5)
        self._merge(m, bm)

    def cyl(self, m, r1, r2, h, x, y, z, seg=24, rot=None):
        bm = bmesh.new()
        bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=seg, radius1=r1, radius2=r2, depth=h,
                              matrix=T(x, y, z) @ (rot or Matrix.Identity(4)))
        self._merge(m, bm)

    def _merge(self, m, bm):
        me = bpy.data.meshes.new("tmp"); bm.to_mesh(me); bm.free()
        self.bms[m].from_mesh(me); bpy.data.meshes.remove(me)

    def finish(self, name, sharp=35):
        mats = [m for m, bm in self.bms.items() if bm.faces]
        me = bpy.data.meshes.new(name)
        big = bmesh.new()
        for i, m in enumerate(mats):
            tmp = bpy.data.meshes.new("t"); self.bms[m].to_mesh(tmp)
            for p in tmp.polygons: p.material_index = i
            big.from_mesh(tmp); bpy.data.meshes.remove(tmp)
        for bm in self.bms.values(): bm.free()
        big.to_mesh(me); big.free()
        for m in mats: me.materials.append(bpy.data.materials.get(m) or bpy.data.materials.new(m))
        me.polygons.foreach_set("use_smooth", [True] * len(me.polygons)); me.set_sharp_from_angle(angle=math.radians(sharp))
        ob = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(ob)
        return ob


def torii():
    """Myōjin torii (jak w Itsukushimie, tylko mniejsza): podwójne nadproże z wygiętymi końcami, nuki, gakuzuka, czarne podstawy.
    Układ legacy: X w poprzek, front w -Y, woda Z=0 (słupy schodzą pod wodę)."""
    b = Builder(["red", "black", "stone"])
    span, H, r = 2.6, 6.2, 0.3
    for s in (-1, 1):
        lean = RY(s * 0.035)
        b.cyl("red", r, r * 0.92, H + 1.5, s * span, 0, (H - 1.5) / 2, seg=28, rot=lean)
        b.cyl("black", r * 1.18, r * 1.18, 1.3, s * span, 0, -0.2, seg=28)             # kamaki — czarna osłona u dołu
        b.cyl("black", r * 1.25, r * 1.25, 0.08, s * span, 0, 0.47, seg=28)
    b.box("red", span * 2 + 1.2, 0.34, 0.36, 0, 0, H - 1.55, bevel=0.03)            # nuki — belka
    b.box("red", 0.32, 0.3, 0.95, 0, 0, H - 0.9, bevel=0.02)                          # gakuzuka
    b.box("black", 0.62, 0.08, 0.5, 0, -0.17, H - 0.9, bevel=0.01)                    # tabliczka (gaku)
    # shimaki + kasagi: dwie belki wygięte do góry na końcach
    n = 28; L = span * 2 + 2.4
    for i in range(n):
        x0 = -L / 2 + L * i / n; x1 = x0 + L / n; xm = (x0 + x1) / 2
        lift = lambda x: 0.32 * (abs(x) / (L / 2)) ** 2.6
        dz = lift(x1) - lift(x0); ang = math.atan2(dz, x1 - x0) if xm > 0 else math.atan2(dz, x1 - x0)
        seglen = math.hypot(x1 - x0, dz) + 0.02
        b.box("red", seglen, 0.42, 0.34, xm, 0, H + 0.0 + lift(xm), rot=RY(-ang))
        b.box("black", seglen, 0.6, 0.26, xm, 0, H + 0.32 + lift(xm) * 1.25, rot=RY(-ang * 1.25))
    return b.finish("SM_Torii")


def float_lantern():
    """Tōrō nagashi: drewniana tacka, cztery słupki, ścianki z papieru (slot 'lanternpaper' — kolor z czatu), daszek."""
    b = Builder(["wood", "lanternpaper", "black"])
    w, h = 0.42, 0.42
    b.box("wood", w + 0.1, w + 0.1, 0.06, 0, 0, 0.03, bevel=0.01)
    for sx in (-1, 1):
        for sy in (-1, 1):
            b.box("wood", 0.03, 0.03, h, sx * w / 2, sy * w / 2, 0.06 + h / 2)
    for s in (-1, 1):
        b.box("lanternpaper", w - 0.02, 0.006, h - 0.04, 0, s * w / 2, 0.06 + h / 2)
        b.box("lanternpaper", 0.006, w - 0.02, h - 0.04, s * w / 2, 0, 0.06 + h / 2)
    b.box("wood", w + 0.04, 0.03, 0.03, 0, -w / 2, 0.06 + h, ); b.box("wood", w + 0.04, 0.03, 0.03, 0, w / 2, 0.06 + h)
    b.box("wood", 0.03, w + 0.04, 0.03, -w / 2, 0, 0.06 + h); b.box("wood", 0.03, w + 0.04, 0.03, w / 2, 0, 0.06 + h)
    b.cyl("black", 0.035, 0.03, 0.08, 0, 0, 0.1, seg=12)                               # świeca
    return b.finish("SM_FloatLantern", sharp=30)


def rock(name, seed, sub=5, scale=(1.6, 1.15, 0.75)):
    rnd = random.Random(seed)
    bm = bmesh.new(); bmesh.ops.create_icosphere(bm, subdivisions=sub, radius=1.0)
    bm.verts.ensure_lookup_table()
    V = np.array([tuple(v.co) for v in bm.verts]); Tr = np.array([[v.index for v in f.verts] for f in bm.faces]); bm.free()
    ph = np.array([rnd.uniform(0, 50) for _ in range(3)])
    def n3(p, f):
        return (noise(p[:, 0] * f + ph[0], p[:, 1] * f + ph[1]) + noise(p[:, 1] * f + ph[2], p[:, 2] * f) + noise(p[:, 2] * f + ph[1], p[:, 0] * f + ph[0])) / 3
    disp = 0.32 * (n3(V, 1.2) - 0.5) + 0.12 * (n3(V, 3.1) - 0.5) + 0.04 * (n3(V, 8.0) - 0.5)
    facet = np.floor((n3(V, 0.9)) * 5) / 5 * 0.08                                     # łamane płaszczyzny
    P = V * (1 + disp + facet)[:, None] * np.array(scale)
    P[:, 2] = np.maximum(P[:, 2], -0.25 * scale[2]) - 0.05                            # płaski spód, kamień leży
    up = np.clip(V[:, 2], 0, 1)
    moss = np.clip((up - 0.45) * 2.2 + (n3(V, 2.0) - 0.5) * 1.5, 0, 1)
    crev = np.clip(0.5 - disp * 3, 0, 1)
    col = np.stack([moss, crev, np.zeros_like(moss), np.ones_like(moss)], 1)
    uv = np.stack([np.arctan2(V[:, 1], V[:, 0]) / TAU * 3, V[:, 2] * 1.5], 1)
    return make_mesh(name, P, Tr, ("rock",), uv=uv, col=col)


def petal():
    """Pojedynczy opadający płatek (3,5 cm), lekko wygięty, z wcięciem na końcu."""
    shape = [(0, 0), (-0.30, 0.42), (-0.20, 0.95), (0.0, 0.84), (0.20, 0.95), (0.30, 0.42)]
    V = np.array([(x * 0.035, y * 0.035, 0.012 * (x / 0.3) ** 2) for x, y in shape])
    Tr = np.array([(0, 1, 2), (0, 2, 3), (0, 3, 4), (0, 4, 5)])
    V[:, 1] -= 0.017
    uv = np.array([(math.hypot(x, y), 0.5 + x) for x, y in shape])
    return make_mesh("SM_Petal", V, Tr, ("petal",), uv=uv)


def rain_streak():
    """Kropla deszczu: dwie skrzyżowane wąskie płaszczyzny, 2 cm × 55 cm, środek w (0,0,0)."""
    w, h = 0.01, 0.55
    V = np.array([(-w, 0, -h / 2), (w, 0, -h / 2), (w, 0, h / 2), (-w, 0, h / 2), (0, -w, -h / 2), (0, w, -h / 2), (0, w, h / 2), (0, -w, h / 2)])
    Tr = np.array([(0, 1, 2), (0, 2, 3), (4, 5, 6), (4, 6, 7)])
    uv = np.array([(0, 0), (1, 0), (1, 1), (0, 1)] * 2)
    return make_mesh("SM_RainStreak", V, Tr, ("rain",), uv=uv)


def spark():
    bm = bmesh.new(); bmesh.ops.create_icosphere(bm, subdivisions=1, radius=0.5)
    V = np.array([tuple(v.co) for v in bm.verts]); Tr = np.array([[v.index for v in f.verts] for f in bm.faces]); bm.free()
    return make_mesh("SM_Spark", V, Tr, ("spark",), uv=np.zeros((len(V), 2)))


if __name__ == "__main__":
    legacy = run_legacy("build_pagoda.py", ["Pagoda"])
    export_fbx([join(legacy["Pagoda"], "SM_Pagoda")], "SM_Pagoda")
    legacy = run_legacy("build_bridge.py", ["Bridge", "Lantern"])
    export_fbx([join(legacy["Bridge"], "SM_Bridge")], "SM_Bridge")
    clear_scene()
    legacy = run_legacy("build_bridge.py", ["Lantern"])
    export_fbx([join(legacy["Lantern"], "SM_StoneLantern")], "SM_StoneLantern")
    for fn in (torii, float_lantern):
        clear_scene(); ob = fn(); legacy_to_ue_axes([ob]); export_fbx([ob], ob.name)
    for i, (seed, sc) in enumerate([(1, (1.6, 1.15, 0.75)), (2, (1.1, 1.0, 0.9)), (3, (2.2, 1.3, 0.6)), (4, (0.9, 0.8, 1.2))]):
        clear_scene(); ob = rock(f"SM_Rock_{i + 1}", seed, scale=sc); export_fbx([ob], ob.name)
    for fn in (petal, rain_streak, spark):
        clear_scene(); ob = fn(); export_fbx([ob], ob.name)
