// Copyright 2016-2018 mik14a / Admix Network. All Rights Reserved.
// Edited by Muppetsg2 2025-2026

using UnrealBuildTool;

public class VOX4U : ModuleRules
{
	public VOX4U(ReadOnlyTargetRules Target) : base(Target)
	{
		// "Latest" exists in every UE 5.x and never triggers a deprecation warning.
		DefaultBuildSettings = BuildSettingsVersion.Latest;
#if UE_5_1_OR_LATER
		// Newest include order; EngineIncludeOrderVersion does not exist in UE 5.0.
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
#endif
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
			}
		);
	}
}
