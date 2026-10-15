# Pagoda pięciopiętrowa do sceny Sakura River — budowana skryptem w Blenderze (bmesh), eksport do GLB.
# Jednostki i układ jak w wersji generowanej w index.html: środek podstawy w (0,0,0), oś Z w górę, front od strony -Y.
import bpy, bmesh, math
from mathutils import Matrix

OUT_DIR = bpy.path.abspath("//") if bpy.data.filepath else None   # nadpisywane przez wywołującego (zmienna OUT)
OUT_DIR = globals().get("OUT", OUT_DIR)

COL = {"stone": 0x8d8f8a, "wall": 0xefe6d2, "red": 0xc8321e, "redDark": 0x8f2014, "roof": 0x33414b,
       "trim": 0x1f282f, "gold": 0xc9a44a, "glow": 0xffc878, "wood": 0x5a3a26}
bms = {k: bmesh.new() for k in COL}

def T(x, y, z): return Matrix.Translation((x, y, z))
def S(sx, sy, sz): return Matrix.Diagonal((sx, sy, sz, 1.0))
RZ = lambda a: Matrix.Rotation(a, 4, "Z")
RX = lambda a: Matrix.Rotation(a, 4, "X")
RY = lambda a: Matrix.Rotation(a, 4, "Y")

def box(mat, sx, sy, sz, x, y, z, rot=None):
    bmesh.ops.create_cube(bms[mat], size=1.0, matrix=T(x, y, z) @ (rot or Matrix.Identity(4)) @ S(sx, sy, sz))

def cyl(mat, r1, r2, h, x, y, z, seg=16, rot=None):
    bmesh.ops.create_cone(bms[mat], cap_ends=True, cap_tris=False, segments=seg, radius1=r1, radius2=r2, depth=h,
                          matrix=T(x, y, z) @ (rot or Matrix.Identity(4)))

def sphere(mat, r, x, y, z, sx=1.0, sy=1.0, sz=1.0, u=14, v=8):
    bmesh.ops.create_uvsphere(bms[mat], u_segments=u, v_segments=v, radius=r, matrix=T(x, y, z) @ S(sx, sy, sz))

def torus(mat, R, r, x, y, z, seg=20, tube=6):
    bm = bms[mat]; rings = []
    for i in range(seg):
        a = 2 * math.pi * i / seg
        rings.append([bm.verts.new((x + (R + r * math.cos(2 * math.pi * j / tube)) * math.cos(a),
                                    y + (R + r * math.cos(2 * math.pi * j / tube)) * math.sin(a),
                                    z + r * math.sin(2 * math.pi * j / tube))) for j in range(tube)])
    for i in range(seg):
        A, B = rings[i], rings[(i + 1) % seg]
        for j in range(tube):
            bm.faces.new((A[j], B[j], B[(j + 1) % tube], A[(j + 1) % tube]))

def tube(mat, pts, r, seg=6, phase=0.0, r_end=None):
    """Rura wzdłuż łamanej (zwężana do r_end), z zaślepkami; normalne na zewnątrz."""
    from mathutils import Vector
    bm = bms[mat]; rings = []; n = len(pts); up = Vector((0, 0, 1))
    P = [Vector(p) for p in pts]
    for i in range(n):
        t = (P[min(i + 1, n - 1)] - P[max(i - 1, 0)]).normalized()
        side = t.cross(up); side = side.normalized() if side.length > 1e-6 else Vector((1, 0, 0))
        n2 = side.cross(t).normalized()
        rr = r if r_end is None else r + (r_end - r) * i / (n - 1)
        rings.append([bm.verts.new(P[i] + rr * (math.cos(phase + 2 * math.pi * j / seg) * side + math.sin(phase + 2 * math.pi * j / seg) * n2)) for j in range(seg)])
    for i in range(n - 1):
        A, B = rings[i], rings[i + 1]
        for j in range(seg):
            bm.faces.new((A[j], B[j], B[(j + 1) % seg], A[(j + 1) % seg]))
    bm.faces.new(rings[0]); bm.faces.new(list(reversed(rings[-1])))

# ---------- dach: wklęsły, z podniesionymi narożnikami ----------
def roof_h(x, y, W, H, r0, lift):
    ax, ay = abs(x), abs(y); m = max(ax, ay)
    v = min(max((m / W - r0) / (1 - r0), 0.0), 1.0)
    corner = (min(ax, ay) / m) if m > 1e-9 else 0.0
    return H * (1 - v) ** 1.7 + lift * corner ** 3 * v * v

def roof(z0, W, H, r0, lift, wall_half, n=28, th=0.16):
    f = lambda x, y: z0 + roof_h(x, y, W, H, r0, lift)
    xs = [-W + 2 * W * i / n for i in range(n + 1)]
    top, bot, rim = bms["roof"], bms["wood"], bms["trim"]
    vt = [[top.verts.new((x, y, f(x, y))) for y in xs] for x in xs]
    vb = [[bot.verts.new((x * 0.985, y * 0.985, f(x, y) - th)) for y in xs] for x in xs]
    for i in range(n):
        for j in range(n):
            top.faces.new((vt[i][j], vt[i + 1][j], vt[i + 1][j + 1], vt[i][j + 1]))
            bot.faces.new((vb[i][j], vb[i][j + 1], vb[i + 1][j + 1], vb[i + 1][j]))
    # czoło okapu
    P = lambda x, y: rim.verts.new((x, y, f(x, y) + 0.015)); Q = lambda x, y: rim.verts.new((x * 0.985, y * 0.985, f(x, y) - th - 0.015))
    for j in range(n):
        a, b = xs[j], xs[j + 1]
        rim.faces.new((P(W, a), Q(W, a), Q(W, b), P(W, b)));  rim.faces.new((P(-W, a), P(-W, b), Q(-W, b), Q(-W, a)))
        rim.faces.new((P(a, W), P(b, W), Q(b, W), Q(a, W)));  rim.faces.new((P(a, -W), Q(a, -W), Q(b, -W), P(b, -W)))
    # naroża: gąsiory wzdłuż przekątnych, lekko wyciągnięte poza okap, z dzwonkiem wietrznym
    s0 = r0 * W
    for sx in (-1, 1):
        for sy in (-1, 1):
            pts = [(sx * s, sy * s, f(sx * s, sy * s) + 0.07) for s in [s0 + (W * 1.03 - s0) * k / 12 for k in range(13)]]
            tube("trim", pts, 0.13, seg=6, r_end=0.09)
            tip = pts[-1]
            sphere("trim", 0.12, tip[0], tip[1], tip[2] + 0.02, u=8, v=6)
            cyl("gold", 0.012, 0.012, 0.3, tip[0], tip[1], tip[2] - 0.2, seg=4)
            cyl("gold", 0.07, 0.035, 0.16, tip[0], tip[1], tip[2] - 0.42, seg=8)
    # rzędy dachówek: wąskie wałki spływające od kalenicy do okapu
    step = 0.55; k = int((W - 0.5) / step)
    for c in [i * step for i in range(-k, k + 1)]:
        a0 = max(s0, abs(c)) + 0.08
        if a0 > W - 0.5: continue
        for sgn in (-1, 1):
            ys = [a0 + (W - 0.02 - a0) * q / 6 for q in range(7)]
            tube("trim", [(c, sgn * y, f(c, sgn * y) + 0.035) for y in ys], 0.055, seg=3, phase=math.pi / 2)
            tube("trim", [(sgn * y, c, f(sgn * y, c) + 0.035) for y in ys], 0.055, seg=3, phase=math.pi / 2)
    # krokwie pod okapem (widać je od dołu)
    k2 = int((W - 0.6) / 0.5)
    for c in [i * 0.5 for i in range(-k2, k2 + 1)]:
        y1 = max(wall_half + 0.12, abs(c) + 0.05); y2 = W - 0.22
        if y2 - y1 < 0.5: continue
        for sgn in (-1, 1):
            z1, z2 = f(c, sgn * y1) - th - 0.07, f(c, sgn * y2) - th - 0.07
            L = math.hypot(y2 - y1, z2 - z1); ang = math.atan2(z2 - z1, y2 - y1)
            box("red", 0.08, L, 0.1, c, sgn * (y1 + y2) / 2, (z1 + z2) / 2, RX(sgn * ang))
            box("red", L, 0.08, 0.1, sgn * (y1 + y2) / 2, c, (z1 + z2) / 2, RY(-sgn * ang))

# ---------- podstawa, schody, latarnie ----------
box("stone", 10.6, 10.6, 0.6, 0, 0, 0.3); box("stone", 9.2, 9.2, 0.5, 0, 0, 0.85)
for k in range(5):
    hgt = 0.22 * (k + 1); box("stone", 2.6, 0.34, hgt, 0, -(4.6 + 0.34 * (5 - k) - 0.17), hgt / 2)
for sx in (-1, 1):
    box("stone", 0.34, 2.0, 0.5, sx * 1.5, -5.45, 0.25)
    x, y = sx * 2.5, -5.9
    cyl("stone", 0.34, 0.28, 0.22, x, y, 0.11, seg=8); cyl("stone", 0.12, 0.1, 0.9, x, y, 0.67, seg=8)
    cyl("stone", 0.2, 0.3, 0.14, x, y, 1.19, seg=8); box("glow", 0.3, 0.3, 0.34, x, y, 1.43)
    cyl("stone", 0.44, 0.06, 0.34, x, y, 1.77, seg=8); sphere("stone", 0.07, x, y, 1.98, u=8, v=6)

# ---------- piętra ----------
NS = 5
widths = [5.6 - 0.75 * i for i in range(NS + 1)]
z = 1.1
for i in range(NS):
    w, h, wn, last = widths[i], 2.2 - 0.1 * i, widths[i + 1], i == NS - 1
    hw = w / 2
    if i > 0:                                   # galeryjka z balustradą
        b = hw + 0.75
        box("wood", 2 * b, 2 * b, 0.1, 0, 0, z + 0.05)
        for s in (-1, 1):
            for off, th_ in ((0.62, 0.07), (0.34, 0.05)):
                box("red", 2 * b, th_, th_, 0, s * b, z + off); box("red", th_, 2 * b, th_, s * b, 0, z + off)
            for k in range(-2, 3):
                tall = 0.72 if abs(k) == 2 else 0.62
                box("red", 0.08, 0.08, tall, k * b / 2, s * b, z + 0.1 + tall / 2)
                if abs(k) != 2: box("red", 0.08, 0.08, tall, s * b, k * b / 2, z + 0.1 + tall / 2)
            for s2 in (-1, 1): sphere("gold", 0.07, s * b, s2 * b, z + 0.88, u=8, v=6)
    box("wall", w - 0.16, w - 0.16, h, 0, 0, z + h / 2)
    if i == 3: box("wood", w - 0.3, w - 0.3, 0.02, 0, 0, z + 0.19)             # drewniana podłoga wnętrza z punktem widokowym
    box("red", w + 0.24, w + 0.24, 0.18, 0, 0, z + 0.09)                     # belka progowa
    box("red", w + 0.3, w + 0.3, 0.22, 0, 0, z + h - 0.11)                   # belka oczepowa
    cols = [-hw, -hw / 3, hw / 3, hw]
    for a in cols:
        for s in (-1, 1):
            cyl("red", 0.15, 0.125, h, a, s * hw, z + h / 2)
            if abs(a) < hw - 1e-6: cyl("red", 0.15, 0.125, h, s * hw, a, z + h / 2)
            # wsporniki (kumimono) nad kolumnami
            for (bx, by, ox, oy) in ((a, s * hw, 0, s), (s * hw, a, s, 0)):
                if (ox != 0 and abs(a) >= hw - 1e-6): continue
                box("redDark", 0.34, 0.34, 0.16, bx, by, z + h + 0.03)
                box("redDark", 0.18 + 0.62 * abs(ox), 0.18 + 0.62 * abs(oy), 0.14, bx + ox * 0.22, by + oy * 0.22, z + h + 0.16)
                box("wall", 0.14, 0.14, 0.1, bx + ox * 0.5, by + oy * 0.5, z + h + 0.27)
    for s in (-1, 1):                                                        # płatew pod krokwiami
        box("red", w + 1.3, 0.14, 0.14, 0, s * (hw + 0.58), z + h + 0.36); box("red", 0.14, w + 1.3, 0.14, s * (hw + 0.58), 0, z + h + 0.36)
    bay = w / 3 - 0.36
    for s in (-1, 1):
        for axis in ("y", "x"):
            def put(mat, sx, sy, sz, u, dpt, zz):       # u = wzdłuż ściany, dpt = odległość od osi
                if axis == "y": box(mat, sx, sy, sz, u, s * dpt, zz)
                else:            box(mat, sy, sx, sz, s * dpt, u, zz)
            d = hw - 0.08
            if i == 0:                                   # parter: dwuskrzydłowe drzwi i świetlik
                dh = h * 0.62
                put("wood", bay / 2 - 0.03, 0.06, dh, -bay / 4, d + 0.02, z + 0.18 + dh / 2)
                put("wood", bay / 2 - 0.03, 0.06, dh, bay / 4, d + 0.02, z + 0.18 + dh / 2)
                put("gold", 0.07, 0.05, 0.07, -0.09, d + 0.07, z + 0.18 + dh * 0.5); put("gold", 0.07, 0.05, 0.07, 0.09, d + 0.07, z + 0.18 + dh * 0.5)
                put("glow", bay, 0.05, h * 0.16, 0, d + 0.02, z + 0.18 + dh + 0.06 + h * 0.08)
                put("red", bay + 0.12, 0.08, 0.07, 0, d + 0.03, z + 0.18 + dh + 0.025)
                for k in range(-2, 3): put("wood", 0.035, 0.07, h * 0.16, k * bay / 5, d + 0.04, z + 0.18 + dh + 0.06 + h * 0.08)
            elif i == 3 and axis == "y" and s == -1:     # czwarte piętro od frontu: otwarte wejście na galerię — stąd patrzy kamera (view=pagoda)
                x0 = hw / 3 - 0.13; y0 = -(d + 0.02); za, zb = z + 0.18, z + h - 0.22
                bt = bms["trim"]                         # ciemny otwór widoczny tylko z zewnątrz; od środka jest przezroczysty
                bt.faces.new([bt.verts.new(q) for q in ((-x0, y0, za), (x0, y0, za), (x0, y0, zb), (-x0, y0, zb))])
            else:                                        # wyżej: okno z listewkami
                wh = h * 0.46; zc = z + h * 0.5
                put("glow", bay, 0.05, wh, 0, d + 0.02, zc)
                put("red", bay + 0.14, 0.08, 0.07, 0, d + 0.03, zc + wh / 2 + 0.035); put("red", bay + 0.14, 0.08, 0.07, 0, d + 0.03, zc - wh / 2 - 0.035)
                for k in range(-3, 4): put("wood", 0.035, 0.07, wh, k * bay / 7, d + 0.04, zc)
            for sb in (-1, 1):                           # boczne przęsła: pas w połowie wysokości
                put("red", w / 3 - 0.2, 0.06, 0.09, sb * w / 3, d + 0.03, z + h * 0.5)
    z += h
    W = hw + 1.95 - i * 0.12; H = 2.7 if last else 1.15
    r0 = 0.03 if last else (wn / 2 + 0.1) / W
    roof(z + 0.3, W, H, r0, 0.9, hw)
    z += 0.3 + H - (0.0 if last else 0.02)

# ---------- iglica (sōrin) ----------
box("gold", 0.62, 0.62, 0.22, 0, 0, z + 0.06)                                # roban
sphere("gold", 0.32, 0, 0, z + 0.17, sz=0.8, u=16, v=10)                      # fukubachi
for k in range(8):                                                           # ukebana — płatki lotosu
    a = k * math.pi / 4
    bmesh.ops.create_uvsphere(bms["gold"], u_segments=8, v_segments=6, radius=0.16,
                              matrix=T(0.26 * math.cos(a), 0.26 * math.sin(a), z + 0.5) @ RZ(a) @ RY(-0.7) @ S(0.5, 1.0, 1.7))
cyl("gold", 0.07, 0.04, 4.5, 0, 0, z + 2.45, seg=10)
for k in range(9):                                                           # kurin — dziewięć pierścieni
    R = 0.47 - k * 0.032; zz = z + 0.95 + k * 0.36
    torus("gold", R, 0.035, 0, 0, zz, seg=20, tube=6)
    for q in range(4): box("gold", R, 0.025, 0.025, R / 2 * math.cos(q * math.pi / 2), R / 2 * math.sin(q * math.pi / 2), zz, RZ(q * math.pi / 2))
for q in range(4):                                                           # suien — „wodny płomień”
    a = q * math.pi / 2 + math.pi / 4
    bmesh.ops.create_uvsphere(bms["gold"], u_segments=8, v_segments=6, radius=0.3,
                              matrix=T(0.2 * math.cos(a), 0.2 * math.sin(a), z + 4.45) @ RZ(a) @ S(0.75, 0.07, 1.5))
sphere("gold", 0.1, 0, 0, z + 4.85, u=10, v=8); sphere("gold", 0.15, 0, 0, z + 5.08, sz=1.15, u=12, v=8)   # ryūsha i hōju
TOP = z + 5.25

# ---------- obiekty: jedna siatka na materiał ----------
def srgb(c): c /= 255.0; return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
for o in [o for o in bpy.data.objects if o.name.startswith("Pagoda_")]: bpy.data.objects.remove(o, do_unlink=True)
if not bpy.data.filepath:
    for nm in ("Cube", "Light", "Camera"):
        o = bpy.data.objects.get(nm)
        if o: bpy.data.objects.remove(o, do_unlink=True)
coll = bpy.data.collections.get("Pagoda") or bpy.data.collections.new("Pagoda")
if coll.name not in bpy.context.scene.collection.children: bpy.context.scene.collection.children.link(coll)
objs, tris = [], {}
for name, hexc in COL.items():
    bm = bms[name]
    mesh = bpy.data.meshes.new("Pagoda_" + name); bm.to_mesh(mesh); bm.free()
    mesh.polygons.foreach_set("use_smooth", [True] * len(mesh.polygons)); mesh.set_sharp_from_angle(angle=math.radians(35)); mesh.update()
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    rgb = (srgb((hexc >> 16) & 255), srgb((hexc >> 8) & 255), srgb(hexc & 255), 1.0)
    mat.diffuse_color = rgb
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = rgb; bsdf.inputs["Roughness"].default_value = 0.8
        if name == "glow": bsdf.inputs["Emission Color"].default_value = rgb; bsdf.inputs["Emission Strength"].default_value = 2.0
    mesh.materials.append(mat)
    ob = bpy.data.objects.new("Pagoda_" + name, mesh); coll.objects.link(ob); objs.append(ob)
    tris[name] = sum(len(p.vertices) - 2 for p in mesh.polygons)
bpy.context.view_layer.update()
result = {"triangles": tris, "total": sum(tris.values()), "height": round(TOP, 2), "objects": [o.name for o in objs]}
