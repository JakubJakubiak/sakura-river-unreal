# Buduje całą treść Sakura River HD w edytorze Unreal (uruchamiane przez Tools/build_content.sh):
#   1) kolekcja parametrów MPC_Sakura (noc, deszcz, wiatr, płatki…), 2) materiały z kodem HLSL z Shaders/Sakura.ush,
#   3) import modeli FBX z Tools/out (Nanite, Nanite Foliage z wokselami dla drzew), 4) mapa SakuraRiver z aktorem świata.
# Skrypt można uruchamiać wielokrotnie — istniejące assety są aktualizowane w miejscu (referencje zostają).
import os, sys, math, traceback
import unreal

ROOT = "/Game/Sakura"
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
FBX_DIR = os.environ.get("SAKURA_OUT") or os.path.join(PROJECT, "Tools", "out")
ONLY = set(filter(None, os.environ.get("SAKURA_ONLY", "").split(",")))     # np. SAKURA_ONLY=materials,meshes,map
MESH_FILTER = set(filter(None, os.environ.get("SAKURA_MESHES", "").split(",")))   # np. SAKURA_MESHES=SM_TerrainFar

AT = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
MSM = unreal.MaterialShadingModel
CMOT = unreal.CustomMaterialOutputType
ERRORS = []


def log(msg):
    unreal.log("[sakura] " + msg)


def err(msg):
    ERRORS.append(msg)
    unreal.log_error("[sakura] " + msg)


def want(step):
    return not ONLY or step in ONLY


def ensure_dir(p):
    if not EAL.does_directory_exist(p):
        EAL.make_directory(p)


def srgb_to_lin(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def hexlin(h):
    return [srgb_to_lin(((h >> s) & 255) / 255.0) for s in (16, 8, 0)]


# ------------------------------------------------------------------ MPC
def build_mpc():
    path = f"{ROOT}/Materials/MPC_Sakura"
    if EAL.does_asset_exist(path):
        mpc = EAL.load_asset(path)
    else:
        mpc = AT.create_asset("MPC_Sakura", f"{ROOT}/Materials", unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    scal = list(mpc.get_editor_property("scalar_parameters"))
    vec = list(mpc.get_editor_property("vector_parameters"))
    have_s = {str(p.get_editor_property("parameter_name")) for p in scal}
    have_v = {str(p.get_editor_property("parameter_name")) for p in vec}
    for name, val in [("Night", 0.0), ("Sunset", 0.0), ("Rain", 0.0), ("Wind", 0.25), ("PetalBoost", 0.0), ("Wet", 0.0), ("EmissiveScale", 900.0)]:
        if name not in have_s:
            p = unreal.CollectionScalarParameter()
            p.set_editor_property("parameter_name", name); p.set_editor_property("default_value", val)
            scal.append(p)
    for name, val in [("SunDir", (0.3, -0.5, 0.8, 0)), ("MoonDir", (0.8, -0.3, 0.4, 0))]:
        if name not in have_v:
            p = unreal.CollectionVectorParameter()
            p.set_editor_property("parameter_name", name); p.set_editor_property("default_value", unreal.LinearColor(*val))
            vec.append(p)
    mpc.set_editor_property("scalar_parameters", scal)
    mpc.set_editor_property("vector_parameters", vec)
    EAL.save_loaded_asset(mpc)
    return mpc


# ------------------------------------------------------------------ materiały
class Graph:
    """Pomocnik do składania grafu materiału z węzłów."""

    def __init__(self, name, mpc, shading=MSM.MSM_DEFAULT_LIT, blend=unreal.BlendMode.BLEND_OPAQUE, two_sided=False,
                 tsn=True, nanite=True, ism=True, is_sky=False, wpo_max=0.0, voxels=False):
        path = f"{ROOT}/Materials/{name}"
        if EAL.does_asset_exist(path):
            self.m = EAL.load_asset(path)
            MEL.delete_all_material_expressions(self.m)
            for e in list(MEL.get_material_expressions(self.m) or []):      # także węzły wyjść (np. Single Layer Water)
                MEL.delete_material_expression(self.m, e)
        else:
            self.m = AT.create_asset(name, f"{ROOT}/Materials", unreal.Material, unreal.MaterialFactoryNew())
        self.name = name
        self.mpc = mpc
        never = unreal.PropertyAccessChangeNotifyMode.NEVER
        for k, v in [("shading_model", shading), ("blend_mode", blend), ("two_sided", two_sided), ("tangent_space_normal", tsn),
                     ("is_sky", is_sky), ("used_with_nanite", nanite), ("used_with_instanced_static_meshes", ism),
                     ("max_world_position_offset_displacement", wpo_max), ("used_with_voxels", voxels)]:
            self.m.set_editor_property(k, v, never)
        self.x, self.y = -1400, -300

    def node(self, cls, **props):
        e = MEL.create_material_expression(self.m, cls, self.x, self.y)
        self.y += 120
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def wp(self): return self.node(unreal.MaterialExpressionWorldPosition)
    def vn(self): return self.node(unreal.MaterialExpressionVertexNormalWS)
    def vc(self): return self.node(unreal.MaterialExpressionVertexColor)
    def uv(self): return self.node(unreal.MaterialExpressionTextureCoordinate)
    def time(self): return self.node(unreal.MaterialExpressionTime)
    def lp(self): return self.node(unreal.MaterialExpressionLocalPosition)
    def rnd(self): return self.node(unreal.MaterialExpressionPerInstanceRandom)
    def cam(self): return self.node(unreal.MaterialExpressionCameraPositionWS)
    def camvec(self): return self.node(unreal.MaterialExpressionCameraVectorWS)
    def pid(self, i): return self.node(unreal.MaterialExpressionPerInstanceCustomData, data_index=i)
    def const(self, v): return self.node(unreal.MaterialExpressionConstant, r=v)
    def scalar(self, name, v): return self.node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=v)
    def vector(self, name, rgba): return self.node(unreal.MaterialExpressionVectorParameter, parameter_name=name, default_value=unreal.LinearColor(*rgba))
    def coll(self, name): return self.node(unreal.MaterialExpressionCollectionParameter, collection=self.mpc, parameter_name=name)

    def custom(self, code, inputs, out_type=CMOT.CMOT_FLOAT3, outputs=(), desc="Sakura"):
        """inputs: [(nazwa, węzeł, nazwa_wyjścia)], outputs: [(nazwa, typ)]"""
        c = self.node(unreal.MaterialExpressionCustom)
        c.set_editor_property("code", code)
        c.set_editor_property("description", desc)
        c.set_editor_property("output_type", out_type)
        ins = []
        for nm, _, _ in inputs:
            ci = unreal.CustomInput(); ci.set_editor_property("input_name", nm); ins.append(ci)
        c.set_editor_property("inputs", ins)
        outs = []
        for nm, t in outputs:
            co = unreal.CustomOutput(); co.set_editor_property("output_name", nm); co.set_editor_property("output_type", t); outs.append(co)
        c.set_editor_property("additional_outputs", outs)
        c.set_editor_property("include_file_paths", ["/Project/Sakura.ush"])
        for nm, e, on in inputs:
            if not MEL.connect_material_expressions(e, on or "", c, nm):
                err(f"{self.name}: nie połączono wejścia {nm}")
        return c

    def out(self, expr, prop, name=""):
        if not MEL.connect_material_property(expr, name, prop):
            err(f"{self.name}: nie połączono {prop} ({name})")

    def finish(self):
        MEL.layout_material_expressions(self.m)
        MEL.recompile_material(self.m)
        EAL.save_loaded_asset(self.m)
        return self.m


MP = unreal.MaterialProperty


def wind_wpo(g, flex, stiff=1.0):
    w = g.custom("return SkWind(WP, LP, T, Wind, Rnd, Flex, Stiff);",
                 [("WP", g.wp(), ""), ("LP", g.lp(), ""), ("T", g.time(), ""), ("Wind", g.coll("Wind"), ""), ("Rnd", g.rnd(), ""),
                  ("Flex", g.const(flex), ""), ("Stiff", g.const(stiff), "")], desc="Wiatr")
    g.out(w, MP.MP_WORLD_POSITION_OFFSET)


def build_materials(mpc):
    mats = {}
    F1, F3 = CMOT.CMOT_FLOAT1, CMOT.CMOT_FLOAT3

    # teren
    g = Graph("M_Terrain", mpc, tsn=False)
    vc = g.vc()
    c = g.custom("float r; float3 n; float3 c = SkTerrain(WP, float4(VC, VA), VN, Cam, Wet, Night, r, n); Rough = r; Nrm = n; return c;",
                 [("WP", g.wp(), ""), ("VC", vc, ""), ("VA", vc, "A"), ("VN", g.vn(), ""), ("Cam", g.cam(), ""), ("Wet", g.coll("Wet"), ""), ("Night", g.coll("Night"), "")],
                 outputs=[("Rough", F1), ("Nrm", F3)], desc="Teren")
    g.out(c, MP.MP_BASE_COLOR); g.out(c, MP.MP_ROUGHNESS, "Rough"); g.out(c, MP.MP_NORMAL, "Nrm")
    mats["terrain"] = g.finish()

    # Fuji
    g = Graph("M_Fuji", mpc)
    vc = g.vc()
    c = g.custom("float r; float3 c = SkFuji(WP, float4(VC, VA), VN, Night, r); Rough = r; return c;",
                 [("WP", g.wp(), ""), ("VC", vc, ""), ("VA", vc, "A"), ("VN", g.vn(), ""), ("Night", g.coll("Night"), "")],
                 outputs=[("Rough", F1)], desc="Fuji")
    g.out(c, MP.MP_BASE_COLOR); g.out(c, MP.MP_ROUGHNESS, "Rough")
    mats["fuji"] = g.finish()

    # woda: Single Layer Water — prawdziwe pochłanianie i rozpraszanie światła w wodzie, odbicia z Lumena (RT na RTX)
    g = Graph("M_Water", mpc, shading=MSM.MSM_SINGLE_LAYER_WATER, tsn=False, nanite=False, ism=False)
    c = g.custom("float f; float3 n = SkWater(WP, T, Rain, Wind, f); Foam = f; Base = float3(0.9, 0.93, 0.95) * f; Rough = lerp(0.035, 0.4, f); return n;",
                 [("WP", g.wp(), ""), ("T", g.time(), ""), ("Rain", g.coll("Rain"), ""), ("Wind", g.coll("Wind"), "")],
                 outputs=[("Foam", F1), ("Base", F3), ("Rough", F1)], desc="Woda")
    g.out(c, MP.MP_NORMAL); g.out(c, MP.MP_BASE_COLOR, "Base"); g.out(c, MP.MP_ROUGHNESS, "Rough"); g.out(c, MP.MP_OPACITY, "Foam")
    slw = g.node(unreal.MaterialExpressionSingleLayerWaterMaterialOutput)
    for pin, node in [("ScatteringCoefficients", g.vector("Scattering", (0.012, 0.035, 0.032, 1))),
                      ("AbsorptionCoefficients", g.vector("Absorption", (0.38, 0.075, 0.06, 1))),
                      ("PhaseG", g.scalar("PhaseG", 0.2)), ("ColorScaleBehindWater", g.scalar("BehindWater", 0.85))]:
        if not MEL.connect_material_expressions(node, "", slw, pin):
            err(f"M_Water: nie połączono {pin}")
    mats["water"] = g.finish()

    # kora
    g = Graph("M_Bark", mpc, wpo_max=120.0, voxels=True)
    vc = g.vc()
    c = g.custom("float r; float3 n; float3 c = SkBark(UV, float4(VC, VA), r, n); Rough = r; Nrm = n; return c;",
                 [("UV", g.uv(), ""), ("VC", vc, ""), ("VA", vc, "A")], outputs=[("Rough", F1), ("Nrm", F3)], desc="Kora")
    g.out(c, MP.MP_BASE_COLOR); g.out(c, MP.MP_ROUGHNESS, "Rough"); g.out(c, MP.MP_NORMAL, "Nrm")
    wind_wpo(g, 0.0)
    mats["bark"] = g.finish()

    # kwiaty wiśni: dwustronne liście (światło przechodzi przez płatki)
    g = Graph("M_Blossom", mpc, shading=MSM.MSM_TWO_SIDED_FOLIAGE, two_sided=True, wpo_max=140.0, voxels=True)
    vc = g.vc()
    c = g.custom("float ao = lerp(0.45, 1.0, VA); float vein = 1.0 - 0.06 * sin(UV.y * 31.4 + UV.x * 12.0);"
                 "Sub = saturate(VC * float3(1.05, 0.9, 0.95)) * 0.85 * ao; Rough = 0.55; return VC * ao * vein;",
                 [("UV", g.uv(), ""), ("VC", vc, ""), ("VA", vc, "A")], outputs=[("Sub", F3), ("Rough", F1)], desc="Kwiat")
    g.out(c, MP.MP_BASE_COLOR); g.out(c, MP.MP_SUBSURFACE_COLOR, "Sub"); g.out(c, MP.MP_ROUGHNESS, "Rough")
    wind_wpo(g, 1.0)
    mats["blossom"] = g.finish()

    # liście (las, młode listki)
    g = Graph("M_Leaf", mpc, shading=MSM.MSM_TWO_SIDED_FOLIAGE, two_sided=True, wpo_max=120.0, voxels=True)
    vc = g.vc()
    c = g.custom("float n = SkFbm(WP.xy * 0.004, 3); float3 c = VC * lerp(0.5, 1.0, VA) * (0.8 + 0.4 * n);"
                 "Sub = c * float3(1.1, 1.3, 0.6); Rough = 0.7; return c;",
                 [("WP", g.wp(), ""), ("VC", vc, ""), ("VA", vc, "A")], outputs=[("Sub", F3), ("Rough", F1)], desc="Liść")
    g.out(c, MP.MP_BASE_COLOR); g.out(c, MP.MP_SUBSURFACE_COLOR, "Sub"); g.out(c, MP.MP_ROUGHNESS, "Rough")
    wind_wpo(g, 0.5, 0.6)
    mats["leaf"] = g.finish()

    # trawa
    g = Graph("M_Grass", mpc, shading=MSM.MSM_TWO_SIDED_FOLIAGE, two_sided=True, wpo_max=40.0)
    vc = g.vc()
    c = g.custom("float n = SkFbm(WP.xy * 0.01, 3); float3 c = VC * (0.8 + 0.4 * n); Sub = c * float3(0.9, 1.1, 0.5); Rough = 0.75; return c;",
                 [("WP", g.wp(), ""), ("VC", vc, "")], outputs=[("Sub", F3), ("Rough", F1)], desc="Trawa")
    g.out(c, MP.MP_BASE_COLOR); g.out(c, MP.MP_SUBSURFACE_COLOR, "Sub"); g.out(c, MP.MP_ROUGHNESS, "Rough")
    vc2 = g.vc()
    w = g.custom("float k = VA * VA; float ph = Rnd * 6.2831 + dot(WP.xy, float2(0.013, 0.011));"
                 "float2 d = normalize(float2(-0.35, 1.0)); float s = sin(T * 1.6 + ph) * 0.7 + sin(T * 2.9 + ph * 1.3) * 0.3;"
                 "return float3(d * s * k * (5.0 + 16.0 * Wind), -abs(s) * k * 2.0);",
                 [("WP", g.wp(), ""), ("VA", vc2, "A"), ("T", g.time(), ""), ("Rnd", g.rnd(), ""), ("Wind", g.coll("Wind"), "")], desc="Wiatr trawy")
    g.out(w, MP.MP_WORLD_POSITION_OFFSET)
    mats["grass"] = g.finish()

    # kamienie
    g = Graph("M_Rock", mpc, tsn=False)
    vc = g.vc()
    c = g.custom("float r; float3 n; float3 c = SkSurface(WP, VN, float3(0.40, 0.39, 0.36), 2.0, 1.4, 0.8, r, n);"
                 "float3 moss = SkSrgb(float3(0.36, 0.45, 0.2)) * (0.75 + 0.5 * SkNoise(WP.xy * 0.02));"
                 "c = lerp(c, moss, saturate(VC.r) * 0.85); c *= lerp(1.0, 0.55, saturate(VC.g) * 0.6);"
                 "float wet = 1.0 - smoothstep(0.0, 35.0, WP.z); c *= lerp(1.0, 0.6, wet); Rough = lerp(r, 0.25, wet); Nrm = n; return c;",
                 [("WP", g.wp(), ""), ("VN", g.vn(), ""), ("VC", vc, "")], outputs=[("Rough", F1), ("Nrm", F3)], desc="Kamień")
    g.out(c, MP.MP_BASE_COLOR); g.out(c, MP.MP_ROUGHNESS, "Rough"); g.out(c, MP.MP_NORMAL, "Nrm")
    mats["rock"] = g.finish()

    # powierzchnie budowli: jeden materiał nadrzędny + instancje na kolory z wersji web
    g = Graph("M_Surface", mpc, tsn=False)
    c = g.custom("float r; float3 n; float3 c = SkSurface(WP, VN, Col, Pat, Scl, R0, r, n); Rough = r; Nrm = n;"
                 "Em = EmCol * ENight * Glow * pow(saturate(Night), 1.5) + EmCol * EDay * Glow; return c;",
                 [("WP", g.wp(), ""), ("VN", g.vn(), ""), ("Col", g.vector("Color", (0.5, 0.5, 0.5, 1)), ""),
                  ("Pat", g.scalar("Pattern", 0.0), ""), ("Scl", g.scalar("Scale", 1.0), ""), ("R0", g.scalar("Roughness", 0.6), ""),
                  ("EmCol", g.vector("Emissive", (1.0, 0.7, 0.4, 1)), ""), ("ENight", g.scalar("EmissiveNight", 0.0), ""),
                  ("EDay", g.scalar("EmissiveDay", 0.0), ""), ("Glow", g.scalar("Glow", 1.0), ""), ("Night", g.coll("Night"), "")],
                 outputs=[("Rough", F1), ("Nrm", F3), ("Em", F3)], desc="Powierzchnia")
    g.out(c, MP.MP_BASE_COLOR); g.out(c, MP.MP_ROUGHNESS, "Rough"); g.out(c, MP.MP_NORMAL, "Nrm"); g.out(c, MP.MP_EMISSIVE_COLOR, "Em")
    g.out(g.scalar("Metallic", 0.0), MP.MP_METALLIC)
    surface = g.finish()
    mats["surface"] = surface
    # klucz: (kolor, wzór, chropowatość, metal, skala, emisja nocą, kolor emisji)
    SURF = {
        "red": (0xc8321e, 7, 0.38, 0, 1, 0, None), "redDark": (0x8f2014, 7, 0.45, 0, 1, 0, None),
        "deck": (0x6e4a33, 1, 0.75, 0, 1, 0, None), "stone": (0x8d8f8a, 2, 0.85, 0, 1, 0, None),
        "black": (0x1b1b1f, 7, 0.42, 0, 1, 0, None), "bronze": (0x8a6a2e, 6, 0.45, 1, 1, 0, None),
        "paper": (0xff7a4a, 5, 0.9, 0, 1, 7, (1.0, 0.5, 0.25)), "glow": (0xffc878, 0, 0.6, 0, 1, 12, (1.0, 0.72, 0.42)),
        "wall": (0xefe6d2, 4, 0.9, 0, 1, 0, None), "roof": (0x33414b, 3, 0.5, 0, 1, 0, None),
        "trim": (0x1f282f, 7, 0.5, 0, 1, 0, None), "gold": (0xc9a44a, 6, 0.3, 1, 1, 0, None),
        "wood": (0x5a3a26, 1, 0.7, 0, 1, 0, None), "lanternpaper": (0xfff1dc, 5, 0.9, 0, 1, 9, (1.0, 0.75, 0.5)),
    }
    for key, (hexc, pat, rough, metal, scale, en, ec) in SURF.items():
        path = f"{ROOT}/Materials/MI_{key}"
        mi = EAL.load_asset(path) if EAL.does_asset_exist(path) else AT.create_asset(f"MI_{key}", f"{ROOT}/Materials", unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        mi.set_editor_property("parent", surface)
        col = hexlin(hexc)
        MEL.set_material_instance_vector_parameter_value(mi, "Color", unreal.LinearColor(col[0], col[1], col[2], 1))
        MEL.set_material_instance_scalar_parameter_value(mi, "Pattern", float(pat))
        MEL.set_material_instance_scalar_parameter_value(mi, "Roughness", float(rough))
        MEL.set_material_instance_scalar_parameter_value(mi, "Metallic", float(metal))
        MEL.set_material_instance_scalar_parameter_value(mi, "Scale", float(scale))
        MEL.set_material_instance_scalar_parameter_value(mi, "EmissiveNight", float(en))
        if ec:
            MEL.set_material_instance_vector_parameter_value(mi, "Emissive", unreal.LinearColor(ec[0], ec[1], ec[2], 1))
        MEL.update_material_instance(mi)
        EAL.save_loaded_asset(mi)
        mats[key] = mi

    # opadające płatki (instancje przesuwane w shaderze wokół kamery)
    g = Graph("M_Petal", mpc, shading=MSM.MSM_TWO_SIDED_FOLIAGE, two_sided=True, nanite=False)
    c = g.custom("float3 c = SkSrgb(float3(1.0, 0.80, 0.86)) * (0.9 + 0.12 * UV.x); Sub = c; Rough = 0.5; return c;",
                 [("UV", g.uv(), "")], outputs=[("Sub", F3), ("Rough", F1)], desc="Płatek")
    g.out(c, MP.MP_BASE_COLOR); g.out(c, MP.MP_SUBSURFACE_COLOR, "Sub"); g.out(c, MP.MP_ROUGHNESS, "Rough")
    w = g.custom("return SkPetalWPO(WP, LP, float3(P0, P1, P2), Cam, T, Rnd, Wind, Boost, Rain);",
                 [("WP", g.wp(), ""), ("LP", g.lp(), ""), ("P0", g.pid(0), ""), ("P1", g.pid(1), ""), ("P2", g.pid(2), ""), ("Cam", g.cam(), ""),
                  ("T", g.time(), ""), ("Rnd", g.rnd(), ""), ("Wind", g.coll("Wind"), ""), ("Boost", g.coll("PetalBoost"), ""), ("Rain", g.coll("Rain"), "")],
                 desc="Ruch płatków")
    g.out(w, MP.MP_WORLD_POSITION_OFFSET)
    mats["petal"] = g.finish()

    # deszcz
    g = Graph("M_Rain", mpc, shading=MSM.MSM_UNLIT, blend=unreal.BlendMode.BLEND_TRANSLUCENT, nanite=False)
    c = g.custom("Op = 0.16 * saturate(UV.y * 4.0) * saturate((1.0 - UV.y) * 4.0); return float3(0.8, 0.86, 0.95) * Em * 1.4;",
                 [("UV", g.uv(), ""), ("Em", g.coll("EmissiveScale"), "")], outputs=[("Op", F1)], desc="Kropla")
    g.out(c, MP.MP_EMISSIVE_COLOR); g.out(c, MP.MP_OPACITY, "Op")
    w = g.custom("return SkRainWPO(WP, LP, float3(P0, P1, P2), Cam, T, Rnd, Rain, Wind);",
                 [("WP", g.wp(), ""), ("LP", g.lp(), ""), ("P0", g.pid(0), ""), ("P1", g.pid(1), ""), ("P2", g.pid(2), ""), ("Cam", g.cam(), ""),
                  ("T", g.time(), ""), ("Rnd", g.rnd(), ""), ("Rain", g.coll("Rain"), ""), ("Wind", g.coll("Wind"), "")], desc="Ruch deszczu")
    g.out(w, MP.MP_WORLD_POSITION_OFFSET)
    mats["rain"] = g.finish()

    # świetliki: addytywne (zgaszone są niewidoczne), rozmiar z danych (instancja ma skalę 1)
    g = Graph("M_Firefly", mpc, shading=MSM.MSM_UNLIT, blend=unreal.BlendMode.BLEND_ADDITIVE, nanite=False)
    c = g.custom("float b = pow(0.5 + 0.5 * sin(T * 2.1 + Rnd * 40.0), 6.0); return float3(0.75, 1.0, 0.35) * b * 6.0 * smoothstep(0.55, 0.85, Night);",
                 [("T", g.time(), ""), ("Rnd", g.rnd(), ""), ("Night", g.coll("Night"), "")], desc="Świetlik")
    g.out(c, MP.MP_EMISSIVE_COLOR)
    w = g.custom("float b = 0.35 + 0.65 * pow(0.5 + 0.5 * sin(T * 2.1 + Rnd * 40.0), 6.0); return SkFireflyWPO(WP, LP * 0.07 * b, float3(P0, P1, P2), T, Rnd, Night);",
                 [("WP", g.wp(), ""), ("LP", g.lp(), ""), ("P0", g.pid(0), ""), ("P1", g.pid(1), ""), ("P2", g.pid(2), ""),
                  ("T", g.time(), ""), ("Rnd", g.rnd(), ""), ("Night", g.coll("Night"), "")], desc="Lot świetlika")
    g.out(w, MP.MP_WORLD_POSITION_OFFSET)
    mats["firefly"] = g.finish()

    # iskry fajerwerków: kolor, chwila narodzin i czas życia w danych instancji
    g = Graph("M_Spark", mpc, shading=MSM.MSM_UNLIT, nanite=False)
    c = g.custom("float age = saturate((T - Birth) / max(Life, 0.01)); float fade = (1.0 - age) * (1.0 - age);"
                 "float fl = 0.7 + 0.3 * sin(T * 37.0 + Rnd * 60.0); return float3(R, G, B) * fade * fl * 70.0 * Em;",
                 [("R", g.pid(0), ""), ("G", g.pid(1), ""), ("B", g.pid(2), ""), ("Birth", g.pid(3), ""), ("Life", g.pid(4), ""),
                  ("T", g.time(), ""), ("Rnd", g.rnd(), ""), ("Em", g.coll("EmissiveScale"), "")], desc="Iskra")
    g.out(c, MP.MP_EMISSIVE_COLOR)
    mats["spark"] = g.finish()

    # niebo: atmosfera (SkyAtmosphere) + tarcza słońca + gwiazdy, Droga Mleczna i księżyc nocą
    g = Graph("M_Sky", mpc, shading=MSM.MSM_UNLIT, two_sided=True, nanite=False, ism=False, is_sky=True)
    sky = g.node(unreal.MaterialExpressionSkyAtmosphereViewLuminance)
    disk = g.node(unreal.MaterialExpressionSkyAtmosphereLightDiskLuminance, light_index=0)
    c = g.custom("float3 d = normalize(-CV); return Sky + Disk + SkNightSky(d, normalize(Moon.xyz), Night, T);",
                 [("CV", g.camvec(), ""), ("Sky", sky, ""), ("Disk", disk, ""), ("Moon", g.coll("MoonDir"), ""), ("Night", g.coll("Night"), ""), ("T", g.time(), "")],
                 desc="Niebo")
    g.out(c, MP.MP_EMISSIVE_COLOR)
    mats["sky"] = g.finish()
    return mats


# ------------------------------------------------------------------ modele
MESHES = {
    # nazwa: (nanite, tryb kształtu Nanite, dwustronne pole odległości, skala rozdzielczości DF)
    "SM_TerrainNear": (True, "NONE", False, 0.25), "SM_TerrainFar": (True, "NONE", False, 0.05), "SM_Fuji": (True, "NONE", False, 0.05),
    "SM_Sakura_A": (True, "VOXELIZE", True, 1.0), "SM_Sakura_B": (True, "VOXELIZE", True, 1.0), "SM_Sakura_C": (True, "VOXELIZE", True, 1.0),
    "SM_Tree_Broad": (True, "VOXELIZE", True, 1.0), "SM_Tree_Sugi": (True, "VOXELIZE", True, 1.0),
    "SM_GrassPatch": (True, "PRESERVE_AREA", True, 0.5),
    "SM_Pagoda": (True, "NONE", False, 1.0), "SM_Bridge": (True, "NONE", False, 1.0), "SM_StoneLantern": (True, "NONE", False, 1.0),
    "SM_Torii": (True, "NONE", False, 1.0), "SM_FloatLantern": (True, "NONE", False, 1.0),
    "SM_Rock_1": (True, "NONE", False, 1.0), "SM_Rock_2": (True, "NONE", False, 1.0), "SM_Rock_3": (True, "NONE", False, 1.0), "SM_Rock_4": (True, "NONE", False, 1.0),
    "SM_Petal": (False, "NONE", False, 0.0), "SM_RainStreak": (False, "NONE", False, 0.0), "SM_Spark": (False, "NONE", False, 0.0),
}


def import_meshes(mats):
    SMES = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.StaticMeshEditorSubsystem()
    ensure_dir(f"{ROOT}/Meshes")
    before = set(EAL.list_assets(f"{ROOT}/Meshes", recursive=True, include_folder=False))
    tasks = []
    names = [n for n in MESHES if not MESH_FILTER or n in MESH_FILTER]
    for name in names:
        f = os.path.join(FBX_DIR, name + ".fbx")
        if not os.path.exists(f):
            err(f"brak pliku {f}")
            continue
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", f)
        t.set_editor_property("destination_path", f"{ROOT}/Meshes")
        t.set_editor_property("destination_name", name)
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("save", False)
        tasks.append(t)
    AT.import_asset_tasks(tasks)
    for name in names:
        nanite, shape, two_sided_df, dfres = MESHES[name]
        path = f"{ROOT}/Meshes/{name}"
        mesh = EAL.load_asset(path)
        if not isinstance(mesh, unreal.StaticMesh):
            err(f"nie zaimportowano {name}")
            continue
        # materiały po nazwach slotów z Blendera
        slots = list(mesh.get_editor_property("static_materials"))
        for s in slots:
            raw = str(s.get_editor_property("material_slot_name"))
            key = raw.split(".")[0]
            for k in mats:
                if key.lower() == k.lower():
                    key = k
            if key in mats:
                s.set_editor_property("material_interface", mats[key])
            else:
                err(f"{name}: brak materiału dla slotu '{raw}'")
        mesh.set_editor_property("static_materials", slots)
        # Nanite (+ woksele dla koron drzew: nowy tryb Nanite Foliage)
        ns = SMES.get_nanite_settings(mesh)
        ns.set_editor_property("enabled", nanite)
        if nanite:
            ns.set_editor_property("shape_preservation", getattr(unreal.NaniteShapePreservation, shape))
        SMES.set_nanite_settings(mesh, ns, apply_changes=False)
        bs = SMES.get_lod_build_settings(mesh, 0)
        bs.set_editor_property("generate_lightmap_u_vs", False)
        bs.set_editor_property("generate_distance_field_as_if_two_sided", two_sided_df)
        bs.set_editor_property("distance_field_resolution_scale", float(dfres))
        SMES.set_lod_build_settings(mesh, 0, bs)
        try:
            SMES.remove_collisions(mesh)
        except Exception:
            pass
        mesh.set_editor_property("allow_cpu_access", False)
        EAL.save_loaded_asset(mesh)
        log(f"{name}: nanite={nanite} ({shape}), sloty={[str(s.get_editor_property('material_slot_name')) for s in slots]}")
    # importer tworzy też własne materiały i tekstury z FBX — niepotrzebne, usuwamy
    after = set(EAL.list_assets(f"{ROOT}/Meshes", recursive=True, include_folder=False))
    for a in sorted(after - before):
        obj_name = a.split(".")[-1]
        if obj_name not in MESHES:
            data = EAL.find_asset_data(a)
            if str(data.asset_class_path.asset_name) != "StaticMesh":
                EAL.delete_asset(a)


# ------------------------------------------------------------------ mapa
def build_map():
    ensure_dir(f"{ROOT}/Maps")
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    path = f"{ROOT}/Maps/SakuraRiver"
    if EAL.does_asset_exist(path):
        les.load_level(path)
    else:
        les.new_level(path)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    cls = unreal.load_class(None, "/Script/SakuraRiver.SakuraWorld")
    have = [a for a in eas.get_all_level_actors() if a.get_class() == cls]
    if not have and cls:
        eas.spawn_actor_from_class(cls, unreal.Vector(0, 0, 0))
        log("dodano SakuraWorld do mapy")
    les.save_current_level()


def main():
    for d in ("Materials", "Meshes", "Maps"):
        ensure_dir(f"{ROOT}/{d}")
    mpc = build_mpc()
    mats = {}
    if want("materials") or want("meshes"):
        mats = build_materials(mpc)
    if want("meshes"):
        import_meshes(mats)
    if want("map"):
        build_map()
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
    log("GOTOWE" + (f" — błędy: {len(ERRORS)}" if ERRORS else " bez błędów"))
    for e in ERRORS:
        log("  ! " + e)


try:
    main()
except Exception:
    err(traceback.format_exc())
