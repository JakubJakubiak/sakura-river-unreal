using UnrealBuildTool;

public class SakuraRiverTarget : TargetRules
{
	public SakuraRiverTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.AddRange(new string[] { "SakuraShaders", "SakuraRiver" });
	}
}
