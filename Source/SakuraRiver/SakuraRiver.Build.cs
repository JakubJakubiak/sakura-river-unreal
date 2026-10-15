using UnrealBuildTool;

public class SakuraRiver : ModuleRules
{
	public SakuraRiver(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore" });
		PrivateDependencyModuleNames.AddRange(new string[] {
			"WebSockets", "Slate", "SlateCore", "UMG", "RenderCore", "RHI", "MediaIOCore", "NDIMedia", "MediaAssets"
		});
		// Sakura.ini obok gry (ustawienia dla streamera)
		RuntimeDependencies.Add("$(ProjectDir)/Sakura.ini", StagedFileType.NonUFS);
	}
}
