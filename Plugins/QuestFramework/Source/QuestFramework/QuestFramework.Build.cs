using UnrealBuildTool;

public class QuestFramework : ModuleRules
{
	public QuestFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"DeveloperSettings"
		});

		// Only to find this plugin's folder, so its test tags (Config/Tags) can be registered in non-shipping builds.
		PrivateDependencyModuleNames.Add("Projects");
	}
}
