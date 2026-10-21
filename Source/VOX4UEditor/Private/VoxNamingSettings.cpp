// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#include "VoxNamingSettings.h"

#define LOCTEXT_NAMESPACE "VoxNamingSettings"

const FString& FVoxNamingPatterns::Get(EVoxAssetType Type) const
{
	switch (Type)
	{
	case EVoxAssetType::Material:         return Material;
	case EVoxAssetType::MaterialInstance: return MaterialInstance;
	case EVoxAssetType::Texture:          return Texture;
	case EVoxAssetType::Voxel:            return Voxel;
	case EVoxAssetType::Blueprint:        return Blueprint;
	case EVoxAssetType::Animation:        return Animation;
	case EVoxAssetType::StaticMesh:
	default:                              return StaticMesh;
	}
}

FVoxNamingPatterns FVoxNamingPatterns::MakeDefault(EVoxNamingConvention Convention)
{
	FVoxNamingPatterns Patterns;
	if (Convention == EVoxNamingConvention::UnrealEngine)
	{
		Patterns.Material = TEXT("M_{BaseName}");
		Patterns.MaterialInstance = TEXT("MI_{BaseName}{_Color}");
		Patterns.Texture = TEXT("T_{BaseName}{_Description}");
		Patterns.StaticMesh = TEXT("SM_{BaseName}{_Color}{_ModelId}");
		Patterns.Voxel = TEXT("V_{BaseName}{_ModelId}");
		Patterns.Blueprint = TEXT("BP_{BaseName}");
		Patterns.Animation = TEXT("VA_{BaseName}");
	}
	else
	{
		Patterns.Material = TEXT("{BaseName}_MT");
		Patterns.MaterialInstance = TEXT("{BaseName}_MI{_Color}");
		Patterns.Texture = TEXT("{BaseName}{Description}_TX");
		Patterns.StaticMesh = TEXT("{BaseName}{_Color}{_ModelId}");
		Patterns.Voxel = TEXT("{BaseName}{_ModelId}");
		Patterns.Blueprint = TEXT("{BaseName}_BP");
		Patterns.Animation = TEXT("{BaseName}_VA");
	}
	return Patterns;
}

UVoxNamingSettings::UVoxNamingSettings()
	: UnrealEngineNames(FVoxNamingPatterns::MakeDefault(EVoxNamingConvention::UnrealEngine))
	, VOX4UNames(FVoxNamingPatterns::MakeDefault(EVoxNamingConvention::VOX4U))
{
}

const FVoxNamingPatterns& UVoxNamingSettings::GetPatterns(EVoxNamingConvention Convention) const
{
	return Convention == EVoxNamingConvention::UnrealEngine ? UnrealEngineNames : VOX4UNames;
}

FName UVoxNamingSettings::GetCategoryName() const
{
	return FName(TEXT("Plugins"));
}

#if WITH_EDITOR

FText UVoxNamingSettings::GetSectionText() const
{
	return LOCTEXT("SectionText", "VOX4U");
}

FText UVoxNamingSettings::GetSectionDescription() const
{
	return LOCTEXT("SectionDescription", "Names of the assets created when importing MagicaVoxel .vox files.");
}

#endif

#undef LOCTEXT_NAMESPACE
