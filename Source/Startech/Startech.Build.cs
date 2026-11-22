// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Startech : ModuleRules
{
	public Startech(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "ProcSector", "DeveloperSettings", "GameplayTags", "QuestFramework", "SaveFramework" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Niagara", "Slate", "SlateCore", "AssetRegistry" });

		// For UHudMaterialCheckLibrary (editor only): the shader platform the HUD material scripts compile for.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("RHI");

			// For UHudLayoutEditLibrary (editor only): editing WB_GameHUD's widget tree from the layout scripts.
			PrivateDependencyModuleNames.AddRange(new string[] { "UMGEditor", "UnrealEd" });
		}

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
