using UnrealBuildTool;

public class SakuraRiverEditorTarget : TargetRules
{
	public SakuraRiverEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "SakuraShaders", "SakuraRiver" });
	}
}
