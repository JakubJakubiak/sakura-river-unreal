using UnrealBuildTool;

// Mały moduł ładowany wcześnie (PostConfigInit): rejestruje folder Shaders/ projektu jako /Project,
// żeby materiały mogły dołączać #include "/Project/Sakura.ush".
public class SakuraShaders : ModuleRules
{
	public SakuraShaders(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "RenderCore", "Projects" });
	}
}
