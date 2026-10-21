// Copyright 2016-2018 mik14a / Admix Network. All Rights Reserved.
// Edited by Muppetsg2 2025-2026

using UnrealBuildTool;

public class VOX4UEditor : ModuleRules
{
	public VOX4UEditor(ReadOnlyTargetRules Target) : base(Target)
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
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"VOX4U",
				"vox4c",            // Source/ThirdParty/vox4c, header only
				"CoreUObject",
				"DeveloperSettings",
				"Projects",
				"Engine",
				"RenderCore",
				"Slate",
				"SlateCore",
				"UnrealEd",
				"AssetTools",
				"AssetRegistry",
				"PropertyEditor",
				"MaterialEditor",
				"MainFrame",
				"MeshDescription",
				"StaticMeshDescription",
				"InputCore",
				"EditorFramework",
				"EditorSubsystem",
				"AdvancedPreviewScene",  // voxel editor viewport
			}
		);
#if !UE_5_1_OR_LATER
		// UE 5.0: tab icons come from FEditorStyle (FAppStyle in 5.1+).
		PrivateDependencyModuleNames.Add("EditorStyle");
#endif
	}
}
