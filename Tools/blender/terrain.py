# Teren Sakura River HD: bliski (0,6 m siatki, koryto rzeki, brzegi, łąki), daleki (40 m, góry do 14 km) i Fuji.
# Kolory wierzchołków (liniowe) to maski dla materiału M_Terrain:
#   R — piasek/żwir przy wodzie, G — dno pod wodą, B — skała na stromiznach, A — odmiana łąki (plamy, kępy kwiatów).
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np
from common import *


def masks(X, Z, H, step):
    d = river_dist(X, Z)
    gz, gx = np.gradient(H, step, step)
    slope = np.sqrt(gx * gx + gz * gz)
    n1 = fbm(X * 0.15 + 4, Z * 0.15 - 2, 3)
    sand = (1 - smooth(0.6, 3.0, d + (n1 - 0.5) * 3.0)) * (1 - smooth(-0.2, -3.2, H))
    bed = smooth(0.1, -0.7, H)
    rock = smooth(0.45, 0.95, slope + (n1 - 0.5) * 0.3)
    meadow = fbm(X * 0.05 + 9, Z * 0.05 + 1, 4)
    return np.stack([sand, bed, rock, meadow], -1)


def grid_mesh(name, x0, x1, z0, z1, step, height_fn, lower_rect=None):
    xs = np.arange(x0, x1 + step * 0.5, step); zs = np.arange(z0, z1 + step * 0.5, step)
    X, Z = np.meshgrid(xs, zs, indexing="xy")
    H = height_fn(X, Z)
    if lower_rect is not None:                         # pod bliskim terenem daleki jest niżej, żeby się nie przebijał
        (ax0, ax1, az0, az1, dz) = lower_rect
        inside = (X > ax0) & (X < ax1) & (Z > az0) & (Z < az1)
        H = np.where(inside, H - dz, H)
    col = masks(X, Z, H, step)
    bx, by, bz = web_to_bl(X, H, Z)
    verts = np.stack([bx.ravel(), by.ravel(), bz.ravel()], 1)
    uv = np.stack([X.ravel() / 8.0, Z.ravel() / 8.0], 1)
    tris = grid_tris(len(xs), len(zs))
    ob = make_mesh(name, verts, tris, ("terrain",), uv=uv, col=col.reshape(-1, 4))
    fix_up_normals(ob)
    return ob


def near_h(X, Z):
    return ground_h(X, Z, hd=True)


def far_h(X, Z):
    # korytarz widoku: na wprost zwykłej kamery (x=0, z=27) góry są niższe, żeby Fuji wystawała nad grzbiety
    ang = np.abs(np.arctan2(X - 0.0, -(Z - 27.0)))
    corridor = 0.35 + 0.65 * smooth(0.05, 0.5, ang)
    return ground_h(X, Z, hd=False) + mountains(X, Z) * corridor


def fuji(name, R=5600.0, H=2400.0, nr=170, na=288):
    """Fuji: wklęsłe zbocza, spłaszczony szczyt z kraterem, żleby (B = żleb, w nim śnieg schodzi niżej)."""
    r = np.linspace(0, 1, nr) ** 1.35
    a = np.linspace(0, TAU, na, endpoint=False)
    Rr, A = np.meshgrid(r, a, indexing="xy")              # wiersze po kącie
    prof = (1 - Rr) ** 2.1 * 0.93 + (1 - Rr) * 0.07
    rim = 0.055
    crater = np.where(Rr < rim, -0.035 * (1 - (Rr / rim) ** 2), 0.0)
    top = np.minimum(prof, (1 - rim) ** 2.1 * 0.93 + (1 - rim) * 0.07)
    gul = ridged(A * 9.0 + 0.3, Rr * 6.0, 4)
    gullies = (gul - 0.5) * 0.035 * smooth(0.04, 0.25, Rr) * (1 - smooth(0.6, 1.0, Rr))
    Hn = (top + crater + gullies) * H
    asym = 1 + 0.05 * np.sin(A * 2 + 0.7) + 0.03 * np.sin(A * 5 + 2.1)
    X = np.cos(A) * Rr * R * asym; Y = np.sin(A) * Rr * R * asym
    verts = np.stack([X.ravel(), Y.ravel(), Hn.ravel()], 1)
    nrr, naa = Rr.shape[1], Rr.shape[0]
    i = np.arange(nrr - 1); j = np.arange(naa)
    I, J = np.meshgrid(i, j, indexing="xy")
    a0 = (J * nrr + I).ravel(); b0 = a0 + 1; c0 = (((J + 1) % naa) * nrr + I).ravel(); d0 = c0 + 1
    tris = np.concatenate([np.stack([a0, b0, c0], 1), np.stack([b0, d0, c0], 1)])
    col = np.stack([np.zeros_like(Rr), np.zeros_like(Rr), smooth(0.45, 0.8, gul), Rr], -1).reshape(-1, 4)
    uv = np.stack([A.ravel() / TAU * 8, Rr.ravel() * 8], 1)
    ob = make_mesh(name, verts, tris, ("fuji",), uv=uv, col=col)
    fix_up_normals(ob)
    return ob


if __name__ == "__main__":
    clear_scene()
    NEAR = (-160.0, 160.0, -300.0, 110.0)
    near = grid_mesh("SM_TerrainNear", *NEAR, 0.6, near_h)
    export_fbx([near], "SM_TerrainNear")
    clear_scene()
    far = grid_mesh("SM_TerrainFar", -6000.0, 6000.0, -14000.0, 2000.0, 40.0, far_h,
                    lower_rect=(NEAR[0] + 2, NEAR[1] - 2, NEAR[2] + 2, NEAR[3] - 2, 2.5))
    export_fbx([far], "SM_TerrainFar")
    clear_scene()
    f = fuji("SM_Fuji")
    export_fbx([f], "SM_Fuji")
