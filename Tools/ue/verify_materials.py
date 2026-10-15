# Kompiluje ponownie wszystkie materiały Sakura i wypisuje znacznik przed każdym — błędy w logu po znaczniku dotyczą końcowego stanu.
import unreal
EAL = unreal.EditorAssetLibrary
for a in sorted(EAL.list_assets("/Game/Sakura/Materials", recursive=False)):
    obj = EAL.load_asset(a)
    if isinstance(obj, unreal.Material):
        unreal.log("[sakura] verify " + obj.get_name())
        unreal.MaterialEditingLibrary.recompile_material(obj)
unreal.log("[sakura] verify koniec")
