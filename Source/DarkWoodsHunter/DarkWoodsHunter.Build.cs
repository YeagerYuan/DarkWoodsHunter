// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DarkWoodsHunter : ModuleRules
{
	public DarkWoodsHunter(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"DarkWoodsHunter",
			"DarkWoodsHunter/Variant_Horror",
			"DarkWoodsHunter/Variant_Horror/UI",
			"DarkWoodsHunter/Variant_Shooter",
			"DarkWoodsHunter/Variant_Shooter/AI",
			"DarkWoodsHunter/Variant_Shooter/UI",
			"DarkWoodsHunter/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
