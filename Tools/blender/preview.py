# Szybki podgląd modelu bez Unreala: wczytuje FBX z Tools/out i renderuje Workbenchem (kolory wierzchołków) do PNG.
# blender -b --factory-startup -P Tools/blender/preview.py -- SM_Sakura_A [SM_Sakura_B ...]
import sys, os, math
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy
from mathutils import Vector
from common import OUT, clear_scene

names = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
for name in names:
    clear_scene()
    bpy.ops.import_scene.fbx(filepath=os.path.join(OUT, name + ".fbx"))
    obs = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    lo = Vector((1e9,) * 3); hi = Vector((-1e9,) * 3)
    for o in obs:
        for c in o.bound_box:
            w = o.matrix_world @ Vector(c); lo = Vector(map(min, lo, w)); hi = Vector(map(max, hi, w))
    ctr = (lo + hi) / 2; size = (hi - lo).length
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam")); bpy.context.scene.collection.objects.link(cam)
    cam.data.lens = 50
    cam.location = ctr + Vector((size * 1.05, -size * 1.05, size * 0.35))
    cam.rotation_euler = (ctr - cam.location).to_track_quat("-Z", "Y").to_euler()
    sc = bpy.context.scene; sc.camera = cam
    sc.render.engine = "BLENDER_WORKBENCH"
    sh = sc.display.shading
    sh.light = "STUDIO"; sh.color_type = "VERTEX"; sh.show_shadows = True; sh.show_cavity = True
    sh.background_type = "VIEWPORT"; sh.background_color = (0.62, 0.72, 0.82)
    sc.render.resolution_x = 900; sc.render.resolution_y = 900
    sc.render.filepath = os.path.join(OUT, "preview_" + name + ".png")
    bpy.ops.render.render(write_still=True)
    print("[sakura] podgląd", sc.render.filepath)
