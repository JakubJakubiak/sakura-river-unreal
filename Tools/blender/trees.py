# Drzewa Sakura River HD — pełna geometria pod Nanite Foliage (bez kart z przezroczystością):
#   SM_Sakura_A/B — Somei-Yoshino: niski pień, szeroka parasolowa korona, dziesiątki tysięcy kwiatów po 5 płatków;
#   SM_Sakura_C — shidarezakura (płacząca wiśnia): gałęzie opadają do ziemi jak kurtyna kwiatów;
#   SM_Tree_Broad, SM_Tree_Sugi — zielone drzewa lasu na wzgórzach (lekkie, instancjonowane tysiącami);
#   SM_GrassPatch — kępa trawy 2×2 m z pojedynczymi źdźbłami.
# Kolor wierzchołka (liniowy): RGB = kolor płatka/liścia, A = zacienienie wnętrza korony (1 = na zewnątrz).
# Materiały (sloty): bark, blossom, leaf, grass.  UV kory: u dookoła gałęzi, v wzdłuż (w metrach / obwód).
import sys, os, math, random
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from mathutils import Vector, Matrix, Quaternion
from common import *

PALS = [[0xffc4d8, 0xffa8c5, 0xffdbe8, 0xf993b6], [0xffe4ee, 0xffd0e0, 0xfff1f6, 0xffbcd4], [0xff9fc0, 0xf77fa8, 0xffb9d2, 0xee6c9a]]


# ---------- geometria gałęzi ----------
class Geo:
    def __init__(self):
        self.v, self.t, self.uv, self.c, self.m = [], [], [], [], []
        self.n = 0

    def add(self, verts, tris, uv, col, mat):
        verts = np.asarray(verts, np.float64); tris = np.asarray(tris, np.int64)
        self.v.append(verts); self.t.append(tris + self.n); self.uv.append(np.asarray(uv)); self.c.append(np.asarray(col))
        self.m.append(np.full(len(tris), mat, np.int32)); self.n += len(verts)

    def arrays(self):
        return (np.concatenate(self.v), np.concatenate(self.t), np.concatenate(self.uv), np.concatenate(self.c), np.concatenate(self.m))


def tube(geo, pts, radii, seg, col, mat=0, v0=0.0):
    """Rura wzdłuż punktów z ramą przenoszoną równolegle (bez skręcania)."""
    P = [Vector(p) for p in pts]; n = len(P)
    rings, uvs = [], []
    a = None; vacc = v0
    for i in range(n):
        t = (P[min(i + 1, n - 1)] - P[max(i - 1, 0)]).normalized()
        if a is None:
            a = t.orthogonal().normalized()
        else:
            a = (a - t * a.dot(t)).normalized()
        b = t.cross(a)
        if i > 0: vacc += (P[i] - P[i - 1]).length / max(TAU * radii[0], 0.05)
        for k in range(seg + 1):
            ang = TAU * k / seg
            rings.append(P[i] + (a * math.cos(ang) + b * math.sin(ang)) * radii[i])
            uvs.append((k / seg, vacc))
    tris = []
    W = seg + 1
    for i in range(n - 1):
        for k in range(seg):
            p0 = i * W + k; p1 = p0 + 1; p2 = p0 + W; p3 = p2 + 1
            tris += [(p0, p2, p1), (p1, p2, p3)]
    geo.add([tuple(r) for r in rings], tris, uvs, np.tile(col, (len(rings), 1)), mat)


# ---------- kwiat: 5 płatków z wcięciem na końcu, lekko miseczkowate, środek z pręcikami ----------
def flower_template():
    pv, pt, puv = [], [], []
    shape = [(0, 0), (-0.30, 0.42), (-0.20, 0.95), (0.0, 0.84), (0.20, 0.95), (0.30, 0.42)]
    for p in range(5):
        ang = TAU * p / 5 + 0.1
        base = len(pv)
        for (sx, sy) in shape:
            r = math.hypot(sx, sy)
            x = sx * math.cos(ang) - (sy + 0.12) * math.sin(ang)
            y = sx * math.sin(ang) + (sy + 0.12) * math.cos(ang)
            z = 0.28 * r * r + 0.06 * r
            pv.append((x, y, z)); puv.append((r, p / 5))
        pt += [(base, base + 1, base + 2), (base, base + 2, base + 3), (base, base + 3, base + 4), (base, base + 4, base + 5)]
    c = len(pv)                                            # środek: mały stożek pręcików
    pv += [(0, 0, 0.2)] + [(0.16 * math.cos(TAU * k / 5), 0.16 * math.sin(TAU * k / 5), 0.02) for k in range(5)]
    puv += [(0, 1)] * 6
    pt += [(c, c + 1 + k, c + 1 + (k + 1) % 5) for k in range(5)]
    is_center = np.zeros(len(pv)); is_center[c:] = 1
    rad = np.array([u for u, _ in puv]); rad[c:] = 0
    return np.array(pv), np.array(pt), np.array(puv), is_center, rad


FV, FT, FUV, FCENTER, FRAD = flower_template()


def leaf_template():
    v = [(0, 0, 0), (-0.32, 0.35, 0.04), (-0.22, 0.75, 0.06), (0, 1.0, 0.02), (0.22, 0.75, 0.06), (0.32, 0.35, 0.04)]
    t = [(0, 1, 2), (0, 2, 3), (0, 3, 4), (0, 4, 5)]
    uv = [(0, 0), (0, 0.35), (0, 0.75), (0, 1), (1, 0.75), (1, 0.35)]
    return np.array(v), np.array(t), np.array(uv)


LV, LT, LUV = leaf_template()


def rot_to(n):
    """Macierz obrotu z osi Z na kierunek n (z losowym obrotem wokół n)."""
    n = Vector(n).normalized()
    q = Vector((0, 0, 1)).rotation_difference(n)
    return q


def scatter(geo, template, centers, normals, sizes, cols, rnd, mat):
    tv, tt, tuv = template[:3]
    N = len(centers)
    if N == 0: return
    M = np.zeros((N, 3, 3))
    for i in range(N):
        q = rot_to(normals[i]) @ Quaternion((0, 0, 1), rnd.uniform(0, TAU))
        M[i] = np.array(q.to_matrix())
    V = np.einsum("nij,vj->nvi", M, tv) * np.asarray(sizes)[:, None, None] + np.asarray(centers)[:, None, :]
    T = (tt[None, :, :] + (np.arange(N) * len(tv))[:, None, None]).reshape(-1, 3)
    UV = np.tile(tuv, (N, 1))
    cols = np.asarray(cols)
    C = np.repeat(cols, len(tv), axis=0) if len(cols) == N else cols      # kolor na instancję albo już na wierzchołek
    geo.add(V.reshape(-1, 3), T, UV, C, mat)


def flower_cols(base_lin, n_flowers, ao, rnd):
    """Kolory na wierzchołki kwiatów: płatek jaśnieje ku krawędzi, środek ciemnoróżowy/purpurowy."""
    cols = np.zeros((n_flowers, len(FV), 4))
    tip = np.array([1.0, 0.93, 0.95])
    eye = srgb_to_lin(hexcol(0xb03060))
    for i in range(n_flowers):
        b = base_lin[i]
        petal = b[None, :] * (1 - FRAD[:, None] * 0.35) + tip[None, :] * FRAD[:, None] * 0.35
        petal = np.where(FCENTER[:, None] > 0, eye[None, :] * 0.8, petal)
        cols[i, :, :3] = petal
        cols[i, :, 3] = ao[i]
    return cols.reshape(n_flowers, -1)


# ---------- wiśnia ----------
def cherry(name, seed, pal, height=6.5, spread=1.0, weeping=False, flowers_per_cluster=(3, 6), density=1.0):
    """Wiśnia: pień → konary → gałęzie → gałązki → krótkie ostrogi z kiściami kwiatów.
    weeping=True: z konarów zwisają długie, cienkie pędy (kurtyna kwiatów), jak w shidarezakura."""
    rnd = random.Random(seed)
    geo = Geo()
    bark = srgb_to_lin(hexcol(0x4a3230)).tolist() + [1.0]
    clusters = []            # (pozycja, kierunek na zewnątrz, zwisająca?)
    crown_c = Vector((0, 0, height * 0.7))
    K = height / 6.5
    LEN = [height * (0.30 if not weeping else 0.42), 2.9 * K, 1.75 * K, 1.0 * K, 0.5 * K]
    NCH = [(5, 6), (4, 5), (4, 5), (3, 4), (0, 0)] if not weeping else [(3, 4), (3, 4), (2, 3), (0, 0), (0, 0)]
    SEG = [16, 12, 8, 6, 4, 4]

    def path(p0, d, length, r0, depth, droop=0.0, outward=0.0, curl=0.10):
        nseg = max(3, int(length / (0.25 if depth < 2 else 0.14))) + 1
        pts, rad = [Vector(p0)], [r0]
        dv = Vector(d).normalized()
        for i in range(1, nseg):
            t = i / (nseg - 1)
            jit = Vector((rnd.gauss(0, 1), rnd.gauss(0, 1), rnd.gauss(0, 1))) * curl
            o = Vector((pts[-1].x, pts[-1].y, 0))
            ow = o.normalized() * outward if o.length > 0.01 else Vector()
            dv = (dv + jit + Vector((0, 0, -droop * t)) + ow).normalized()
            pts.append(pts[-1] + dv * length / (nseg - 1))
            rad.append(max(0.005, r0 * (1 - 0.6 * t)))
        return pts, rad

    def add_clusters(pts, step, hang=False, prob=0.95):
        L = 0.0
        for i in range(1, len(pts)):
            L += (pts[i] - pts[i - 1]).length
            if L >= step:
                L = 0.0
                if rnd.random() < prob * density:
                    clusters.append((pts[i].copy(), (pts[i] - crown_c).normalized(), hang))

    def spurs(pts, rad, n):
        """Krótkie ostrogi (10–18 cm) z kiścią na końcu — tak kwitnie wiśnia."""
        for _ in range(n):
            idx = rnd.randrange(1, len(pts))
            tan = (pts[idx] - pts[idx - 1]).normalized()
            side = tan.orthogonal().normalized(); side.rotate(Quaternion(tan, rnd.uniform(0, TAU)))
            dd = (side + Vector((0, 0, 0.4))).normalized()
            end = pts[idx] + dd * rnd.uniform(0.08, 0.16)
            tube(geo, [pts[idx], end], [rad[idx] * 0.6, 0.004], 3, bark, 0)
            clusters.append((end, (end - crown_c).normalized(), False))

    def strand(p0):
        """Zwisający pęd płaczącej wiśni."""
        o = Vector((p0.x, p0.y, 0)); o = o.normalized() if o.length > 0.01 else Vector((1, 0, 0))
        o.rotate(Quaternion((0, 0, 1), rnd.uniform(-0.7, 0.7)))
        dv = (o + Vector((0, 0, rnd.uniform(0.1, 0.5)))).normalized()
        pts = [p0.copy()]; L = rnd.uniform(2.2, 4.2) * K; step = 0.14
        for i in range(int(L / step)):
            dv = (dv + Vector((0, 0, -0.32)) + Vector((rnd.gauss(0, 0.03), rnd.gauss(0, 0.03), 0))).normalized()
            nxt = pts[-1] + dv * step
            if nxt.z < 0.45: break
            pts.append(nxt)
        if len(pts) < 3: return
        tube(geo, pts, [0.012 * (1 - 0.7 * i / len(pts)) + 0.003 for i in range(len(pts))], 3, bark, 0)
        add_clusters(pts, 0.075, hang=True, prob=0.97)

    def grow(p0, d, r0, depth):
        length = LEN[depth] * rnd.uniform(0.85, 1.15)
        if depth == 0:
            pts, rad = path(p0, d, length, r0, 0, curl=0.04)
            rad = [r * (1 + 0.7 * max(0, 1 - i / 2.5)) for i, r in enumerate(rad)]    # rozszerzenie pnia przy ziemi
        elif weeping:
            pts, rad = path(p0, d, length, r0, depth, droop=0.05, outward=0.05, curl=0.08)
        else:
            pts, rad = path(p0, d, length, r0, depth, droop=(-0.04 if depth == 1 else 0.035 * depth), outward=0.05 if depth <= 2 else 0.0, curl=0.07 + 0.035 * depth)
        tube(geo, pts, rad, SEG[depth], bark, 0)
        if weeping:
            if depth >= 1:
                for _ in range(rnd.randint(7, 11)):
                    strand(pts[rnd.randrange(len(pts) // 3, len(pts))])
            if depth == 2:
                add_clusters(pts, 0.12)
        else:
            if depth >= 3:
                add_clusters(pts, 0.1)
                spurs(pts, rad, 3 if depth == 4 else 4)
        lo, hi = NCH[depth]
        if hi == 0:
            return
        nch = rnd.randint(lo, hi)
        golden = rnd.uniform(0, TAU)
        for k in range(nch):
            t = rnd.uniform(0.8, 1.0) if depth == 0 else 0.25 + 0.75 * (k + rnd.random()) / nch
            idx = min(len(pts) - 1, max(1, int(t * (len(pts) - 1))))
            tan = (pts[idx] - pts[idx - 1]).normalized()
            if depth == 0:
                az = golden + k * TAU / nch + rnd.uniform(-0.35, 0.35)
                ang = math.radians(rnd.uniform(30, 52) if not weeping else rnd.uniform(28, 45))
                cd = Vector((math.cos(az) * math.sin(ang), math.sin(az) * math.sin(ang), math.cos(ang)))
            else:
                perp = tan.orthogonal().normalized(); perp.rotate(Quaternion(tan, golden + k * 2.39996))
                ang = math.radians(rnd.uniform(28, 50))
                cd = tan.copy(); cd.rotate(Quaternion(tan.cross(perp).normalized(), ang))
            grow(pts[idx], cd, rad[idx] * rnd.uniform(0.6, 0.75), depth + 1)

    lean = Vector((rnd.uniform(-0.12, 0.12), rnd.uniform(-0.12, 0.12), 1)).normalized()
    root_r = 0.24 * spread * K
    grow(Vector((0, 0, -0.3)), lean, root_r, 0)
    for k in range(5):                                         # płytkie korzenie przy ziemi, schodzą pod teren
        a = TAU * k / 5 + rnd.uniform(-0.3, 0.3)
        d = Vector((math.cos(a), math.sin(a), 0))
        tube(geo, [Vector((0, 0, 0.35)), d * 0.35 * K + Vector((0, 0, 0.08)), d * 0.75 * K + Vector((0, 0, -0.25))],
             [root_r * 0.8, root_r * 0.55, root_r * 0.35], 8, bark, 0)

    pal_lin = [srgb_to_lin(hexcol(c)) for c in pal]
    centers, normals, sizes, bases, aos = [], [], [], [], []
    leaf_c, leaf_n, leaf_s, leaf_col = [], [], [], []
    P = np.array([tuple(c[0]) for c in clusters]); cc = P.mean(0)
    ext = np.percentile(np.linalg.norm((P - cc) * np.array([1, 1, 1.6]), axis=1), 90)
    for (p, outd, hang) in clusters:
        nf = rnd.randint(*flowers_per_cluster)
        depth_in = np.linalg.norm((np.array(tuple(p)) - cc) * np.array([1, 1, 1.6])) / max(ext, 1e-3)
        ao = float(np.clip(0.35 + 0.7 * depth_in, 0.35, 1.0))
        cbase = pal_lin[rnd.randrange(4)]
        for f in range(nf):
            off = Vector((rnd.gauss(0, 1), rnd.gauss(0, 1), rnd.gauss(0, 1))) * 0.045
            off.z -= abs(rnd.gauss(0, 0.03)) + (0.03 if hang else 0.0)        # kwiaty zwisają na szypułkach
            pos = p + off
            if hang:
                n = (off.normalized() + Vector((outd.x, outd.y, 0)) * 0.6 + Vector((0, 0, -0.4))).normalized()
            else:
                n = (outd * 0.7 + off.normalized() * 0.8 + Vector((0, 0, 0.3))).normalized()
            centers.append(tuple(pos)); normals.append(tuple(n)); sizes.append(rnd.uniform(0.05, 0.065) * (1.15 if hang else 1.0))
            bases.append(np.clip(cbase * rnd.uniform(0.92, 1.05), 0, 1)); aos.append(ao)
        if rnd.random() < 0.22:                                     # młode, czerwonawo-zielone listki
            for f in range(rnd.randint(1, 2)):
                off = Vector((rnd.gauss(0, 1), rnd.gauss(0, 1), rnd.gauss(0, 1))) * 0.06
                leaf_c.append(tuple(p + off)); leaf_n.append(tuple((outd + off.normalized()).normalized()))
                leaf_s.append(rnd.uniform(0.06, 0.09))
                lc = srgb_to_lin(hexcol(rnd.choice([0x8a5a2a, 0x7a7a2c, 0x9b4a2e, 0x6f8a33])))
                leaf_col.append(np.concatenate([lc, [ao]]))
    cols = flower_cols(np.array(bases), len(bases), np.array(aos), rnd)
    scatter(geo, (FV, FT, FUV), centers, normals, sizes, cols.reshape(-1, 4), rnd, 1)
    if leaf_c:
        scatter(geo, (LV, LT, LUV), leaf_c, leaf_n, leaf_s, np.array(leaf_col), rnd, 2)
    V, T, UV, C, M = geo.arrays()
    bx, by, bz = web_to_bl(V[:, 0], V[:, 2], -V[:, 1])     # lokalnie: x w prawo, y w głąb, z w górę → układ UE
    verts = np.stack([bx, by, bz], 1)
    ob = make_mesh(name, verts, T, ("bark", "blossom", "leaf"), tri_mat=M, uv=UV, col=C)
    print(f"[sakura] {name}: kiście {len(clusters)}, kwiaty {len(centers)}")
    return ob


# ---------- zielone drzewa lasu ----------
def blob(geo, c, r, rnd, col, mat, sub=2, squash=0.8):
    """Kłąb liści: zgnieciona ikosfera z szumem (gęsta, matowa bryła listowia)."""
    import bmesh
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=sub, radius=1.0)
    vs = np.array([tuple(v.co) for v in bm.verts]); ts = np.array([[v.index for v in f.verts] for f in bm.faces]); bm.free()
    ph = rnd.uniform(0, 100)
    k = 1 + 0.28 * (np.sin(vs[:, 0] * 3.1 + ph) * np.sin(vs[:, 1] * 2.7 + ph * 0.7) * np.sin(vs[:, 2] * 3.3 + ph * 1.3))
    k += np.array([rnd.uniform(-0.08, 0.08) for _ in range(len(vs))])
    V = vs * k[:, None] * r; V[:, 2] *= squash; V += np.array(c)
    up = np.clip(vs[:, 2] * 0.5 + 0.5, 0, 1)
    C = np.concatenate([np.tile(col[:3], (len(vs), 1)) * (0.75 + 0.35 * up)[:, None], (0.55 + 0.45 * up)[:, None]], 1)
    UV = np.stack([np.arctan2(vs[:, 1], vs[:, 0]) / TAU, vs[:, 2]], 1)
    geo.add(V, ts, UV, C, mat)


def broad_tree(name, seed):
    rnd = random.Random(seed); geo = Geo()
    bark = srgb_to_lin(hexcol(0x3e3428)).tolist() + [1.0]
    tube(geo, [(0, 0, -0.3), (0.1, 0, 2.0), (0.05, 0.1, 4.0)], [0.22, 0.17, 0.08], 8, bark, 0)
    greens = [0x3f6b34, 0x4c7a3a, 0x5a8442, 0x36602f]
    for i in range(9):
        a = rnd.uniform(0, TAU); rr = rnd.uniform(0.3, 1.6); h = rnd.uniform(3.2, 6.2)
        g = srgb_to_lin(hexcol(rnd.choice(greens)))
        blob(geo, (math.cos(a) * rr, math.sin(a) * rr, h), rnd.uniform(1.2, 2.0), rnd, g, 2, sub=3)
    V, T, UV, C, M = geo.arrays()
    bx, by, bz = web_to_bl(V[:, 0], V[:, 2], -V[:, 1])
    return make_mesh(name, np.stack([bx, by, bz], 1), T, ("bark", "blossom", "leaf"), tri_mat=M, uv=UV, col=C)


def sugi_tree(name, seed):
    """Cedr japoński (sugi): smukły stożek z warstw igieł."""
    rnd = random.Random(seed); geo = Geo()
    bark = srgb_to_lin(hexcol(0x4a3426)).tolist() + [1.0]
    H = rnd.uniform(11, 15)
    tube(geo, [(0, 0, -0.3), (0, 0, H * 0.5), (0, 0, H)], [0.3, 0.18, 0.03], 8, bark, 0)
    g = srgb_to_lin(hexcol(0x2f5230))
    n = 11
    for i in range(n):
        t = i / (n - 1); h = H * (0.22 + 0.76 * t); r = (1 - t) * 2.3 + 0.35
        for k in range(3):
            a = rnd.uniform(0, TAU); o = r * 0.35
            blob(geo, (math.cos(a) * o, math.sin(a) * o, h), r * rnd.uniform(0.75, 0.95), rnd, g * rnd.uniform(0.85, 1.1), 2, sub=2, squash=0.55)
    V, T, UV, C, M = geo.arrays()
    bx, by, bz = web_to_bl(V[:, 0], V[:, 2], -V[:, 1])
    return make_mesh(name, np.stack([bx, by, bz], 1), T, ("bark", "blossom", "leaf"), tri_mat=M, uv=UV, col=C)



# ---------- drzewa lasu z prawdziwymi liśćmi (Nanite Foliage, z daleka woksele) ----------
def needle_template():
    """Kępka igieł sugi: wydłużony, wąski liść zgięty w łuk."""
    v = [(0, 0, 0), (-0.12, 0.3, 0.05), (-0.08, 0.7, 0.1), (0, 1.0, 0.06), (0.08, 0.7, 0.1), (0.12, 0.3, 0.05)]
    t = [(0, 1, 2), (0, 2, 3), (0, 3, 4), (0, 4, 5)]
    uv = [(0, 0), (0, 0.3), (0, 0.7), (0, 1), (1, 0.7), (1, 0.3)]
    return np.array(v), np.array(t), np.array(uv)


NV, NT, NUV = needle_template()


def forest_tree(name, seed, kind="broad"):
    rnd = random.Random(seed)
    geo = Geo()
    bark = srgb_to_lin(hexcol(0x4a3a2c if kind == "broad" else 0x5a3a28)).tolist() + [1.0]
    tips = []                                   # (pozycja, kierunek na zewnątrz)
    if kind == "broad":
        H = rnd.uniform(10.0, 12.5)
        greens = [0x4f7a34, 0x5c8a3a, 0x6b9440, 0x3f6a2e, 0x7a9a44]
        crown_c = Vector((0, 0, H * 0.65))

        def grow(p0, d, length, r0, depth):
            n = max(3, int(length / 0.4)) + 1
            pts, rad = [Vector(p0)], [r0]
            dv = Vector(d).normalized()
            for i in range(1, n):
                t = i / (n - 1)
                j = 0.03 if depth == 0 else 0.1
                dv = (dv + Vector((rnd.gauss(0, j), rnd.gauss(0, j), rnd.gauss(0, j) + (0.06 if depth < 2 else -0.02)))).normalized()
                pts.append(pts[-1] + dv * length / (n - 1)); rad.append(max(0.008, r0 * (1 - 0.65 * t)))
            tube(geo, pts, rad, [12, 8, 5, 4, 3][depth], bark, 0)
            if depth >= 2:
                for i in range(1, len(pts)):
                    tips.append((pts[i], (pts[i] - crown_c).normalized()))
            if depth >= 4: return
            nch = [5, 4, 4, 3][depth]
            for k in range(nch):
                t = rnd.uniform(0.55, 1.0) if depth == 0 else 0.3 + 0.7 * (k + rnd.random()) / nch
                idx = min(len(pts) - 1, max(1, int(t * (len(pts) - 1))))
                tan = (pts[idx] - pts[idx - 1]).normalized()
                if depth == 0:
                    az = k * TAU / nch + rnd.uniform(-0.4, 0.4); ang = math.radians(rnd.uniform(30, 55))
                    cd = Vector((math.cos(az) * math.sin(ang), math.sin(az) * math.sin(ang), math.cos(ang)))
                else:
                    perp = tan.orthogonal().normalized(); perp.rotate(Quaternion(tan, rnd.uniform(0, TAU)))
                    cd = tan.copy(); cd.rotate(Quaternion(tan.cross(perp).normalized(), math.radians(rnd.uniform(30, 55))))
                grow(pts[idx], cd, length * rnd.uniform(0.5, 0.7), rad[idx] * 0.65, depth + 1)

        grow(Vector((0, 0, -0.4)), Vector((rnd.uniform(-0.05, 0.05), rnd.uniform(-0.05, 0.05), 1)), H * 0.45, 0.28, 0)
        leaves_c, leaves_n, leaves_s, leaves_col = [], [], [], []
        P = np.array([tuple(t[0]) for t in tips]); cc = P.mean(0); ext = np.percentile(np.linalg.norm(P - cc, axis=1), 90)
        for (p, outd) in tips:
            ao = float(np.clip(0.35 + 0.7 * np.linalg.norm(np.array(tuple(p)) - cc) / max(ext, 1e-3), 0.35, 1.0))
            g = srgb_to_lin(hexcol(rnd.choice(greens)))
            for f in range(rnd.randint(9, 14)):
                off = Vector((rnd.gauss(0, 1), rnd.gauss(0, 1), rnd.gauss(0, 1))) * 0.3
                leaves_c.append(tuple(p + off)); leaves_n.append(tuple((outd * 0.7 + off.normalized() + Vector((0, 0, 0.4))).normalized()))
                leaves_s.append(rnd.uniform(0.18, 0.28))
                leaves_col.append(np.concatenate([g * rnd.uniform(0.85, 1.12), [ao]]))
        scatter(geo, (LV, LT, LUV), leaves_c, leaves_n, leaves_s, np.array(leaves_col), rnd, 2)
    else:
        # sugi: prosty pień do czubka, poziome okółki krótkich gałęzi z kępkami igieł, stożkowa sylwetka
        H = rnd.uniform(17.0, 22.0)
        tube(geo, [Vector((0, 0, -0.5)), Vector((0, 0, H * 0.5)), Vector((rnd.uniform(-0.1, 0.1), rnd.uniform(-0.1, 0.1), H))], [0.36, 0.22, 0.03], 10, bark, 0)
        needles_c, needles_n, needles_s, needles_col = [], [], [], []
        dark = [0x2c4a2c, 0x34522f, 0x27432a, 0x3a5a33]
        z = H * 0.28
        while z < H - 0.3:
            t = (z - H * 0.28) / (H * 0.72)
            R = (1 - t) ** 1.1 * 2.6 + 0.25
            nb = rnd.randint(7, 10)
            for b in range(nb):
                az = b * TAU / nb + rnd.uniform(-0.3, 0.3)
                d = Vector((math.cos(az), math.sin(az), rnd.uniform(-0.15, 0.1))).normalized()
                pts = [Vector((0, 0, z)), Vector((0, 0, z)) + d * R * 0.5 + Vector((0, 0, -0.1)), Vector((0, 0, z)) + d * R + Vector((0, 0, -0.25))]
                tube(geo, pts, [0.05 * (1 - t) + 0.015, 0.02, 0.008], 4, bark, 0)
                for q in range(int(10 + 16 * R)):
                    u = rnd.uniform(0.25, 1.0)
                    p = pts[0].lerp(pts[2], u) + Vector((rnd.gauss(0, 0.18), rnd.gauss(0, 0.18), rnd.gauss(0, 0.12) + 0.1))
                    ao = 0.45 + 0.55 * u
                    g = srgb_to_lin(hexcol(rnd.choice(dark)))
                    for k in range(5):
                        n = (d * 0.6 + Vector((rnd.gauss(0, 0.6), rnd.gauss(0, 0.6), rnd.uniform(0.2, 0.9)))).normalized()
                        needles_c.append(tuple(p)); needles_n.append(tuple(n)); needles_s.append(rnd.uniform(0.3, 0.45))
                        needles_col.append(np.concatenate([g * rnd.uniform(0.85, 1.15), [ao]]))
            z += rnd.uniform(0.38, 0.55)
        scatter(geo, (NV, NT, NUV), needles_c, needles_n, needles_s, np.array(needles_col), rnd, 2)
    V, T, UV, C, M = geo.arrays()
    bx, by, bz = web_to_bl(V[:, 0], V[:, 2], -V[:, 1])
    ob = make_mesh(name, np.stack([bx, by, bz], 1), T, ("bark", "blossom", "leaf"), tri_mat=M, uv=UV, col=C)
    print(f"[sakura] {name}: {len(T):,} trójkątów")
    return ob


def grass_patch(name, seed, n=340, size=2.0):
    """Kępa trawy: zakrzywione, zwężające się źdźbła (5 trójkątów), u nasady ciemniejsze."""
    rnd = random.Random(seed); geo = Geo()
    vs, ts, uvs, cs = [], [], [], []
    greens = [srgb_to_lin(hexcol(c)) for c in (0x6f9a45, 0x7fa84e, 0x5d8a3c, 0x8db457, 0x9aaa55)]
    for b in range(n):
        x, y = rnd.uniform(-size / 2, size / 2), rnd.uniform(-size / 2, size / 2)
        h = rnd.uniform(0.18, 0.45); w = rnd.uniform(0.012, 0.022)
        a = rnd.uniform(0, TAU); bend = rnd.uniform(0.05, 0.25)
        dx, dy = math.cos(a), math.sin(a); px, py = -dy, dx
        g = greens[rnd.randrange(len(greens))] * rnd.uniform(0.85, 1.1)
        base = len(vs)
        for i, t in enumerate((0, 0.35, 0.7, 1.0)):
            ww = w * (1 - t * 0.9); cx = x + dx * bend * t * t; cy = y + dy * bend * t * t; cz = h * t - 0.03
            if t < 1.0:
                vs += [(cx - px * ww, cy - py * ww, cz), (cx + px * ww, cy + py * ww, cz)]
                uvs += [(0, t), (1, t)]; cs += [np.concatenate([g * (0.55 + 0.45 * t), [t]])] * 2
            else:
                vs.append((cx, cy, cz)); uvs.append((0.5, 1)); cs.append(np.concatenate([g, [1.0]]))
        ts += [(base, base + 1, base + 2), (base + 1, base + 3, base + 2), (base + 2, base + 3, base + 4), (base + 3, base + 5, base + 4), (base + 4, base + 5, base + 6)]
    geo.add(vs, ts, uvs, cs, 3)
    V, T, UV, C, M = geo.arrays()
    bx, by, bz = web_to_bl(V[:, 0], V[:, 2], -V[:, 1])
    return make_mesh(name, np.stack([bx, by, bz], 1), T, ("bark", "blossom", "leaf", "grass"), tri_mat=M, uv=UV, col=C)


if __name__ == "__main__":
    only = os.environ.get("SAKURA_ONLY", "")
    jobs = [
        ("SM_Sakura_A", lambda: cherry("SM_Sakura_A", 11, PALS[0], height=6.6, spread=1.0, flowers_per_cluster=(4, 7))),
        ("SM_Sakura_B", lambda: cherry("SM_Sakura_B", 41, PALS[1], height=7.4, spread=1.1, flowers_per_cluster=(4, 7))),
        ("SM_Sakura_C", lambda: cherry("SM_Sakura_C", 37, PALS[1], height=6.2, spread=1.0, weeping=True, flowers_per_cluster=(2, 3))),
        ("SM_Tree_Broad", lambda: forest_tree("SM_Tree_Broad", 5, "broad")),
        ("SM_Tree_Sugi", lambda: forest_tree("SM_Tree_Sugi", 7, "sugi")),
        ("SM_GrassPatch", lambda: grass_patch("SM_GrassPatch", 3)),
    ]
    for name, fn in jobs:
        if only and name not in only.split(","): continue
        clear_scene()
        ob = fn()
        export_fbx([ob], name)
