// Copyright 2016-2018 mik14a / Admix Network. All Rights Reserved.
// Edited by Muppetsg2 2025-2026

#include "VoxAssetImportData.h"

UVoxAssetImportData::UVoxAssetImportData()
	: VoxImportType(EVoxImportType::StaticMesh)
	, BaseName()
	, ModelIndex(INDEX_NONE)
	, AnimationFrame(INDEX_NONE)
	, AssetsNamingConvention(EVoxNamingConvention::UnrealEngine)
	, ResourcesSaveLocation(EVoxResourcesSaveLocation::SubFolder)
	, bImportXForward(true)
	, Pivot(EVoxPivot::BottomCenter)
	, bImportHidden(false)
	, Frame(0)
	, Scale(1.0f)
	, bGenerateLightmapUVs(true)
	, bImportMaterial(true)
	, bOneMaterial(false)
	, bPaletteToTexture(false)
{
}

FVoxImportSettings UVoxAssetImportData::ToSettings() const
{
	FVoxImportSettings Settings;
	Settings.ImportType = VoxImportType;
	Settings.NamingConvention = AssetsNamingConvention;
	Settings.ResourcesSaveLocation = ResourcesSaveLocation;
	Settings.bImportXForward = bImportXForward;
	Settings.Pivot = Pivot;
	Settings.bSeparateModels = ModelIndex >= 0;
	Settings.bImportHidden = bImportHidden;
	Settings.Frame = FMath::Max(0, Frame);
	Settings.Scale = FMath::Max(Scale, 0.001f);
	Settings.bGenerateLightmapUVs = bGenerateLightmapUVs;
	Settings.bImportMaterial = bImportMaterial;
	Settings.bOneMaterial = bOneMaterial;
	Settings.bPaletteToTexture = bPaletteToTexture;
	return Settings;
}

void UVoxAssetImportData::FromSettings(const FVoxImportSettings& Settings, const FString& InBaseName, int32 InModelIndex, int32 InAnimationFrame)
{
	VoxImportType = Settings.ImportType;
	BaseName = InBaseName;
	ModelIndex = InModelIndex;
	AnimationFrame = InAnimationFrame;
	AssetsNamingConvention = Settings.NamingConvention;
	ResourcesSaveLocation = Settings.ResourcesSaveLocation;
	bImportXForward = Settings.bImportXForward;
	Pivot = Settings.Pivot;
	bImportHidden = Settings.bImportHidden;
	Frame = Settings.Frame;
	Scale = Settings.Scale;
	bGenerateLightmapUVs = Settings.bGenerateLightmapUVs;
	bImportMaterial = Settings.bImportMaterial;
	bOneMaterial = Settings.bOneMaterial;
	bPaletteToTexture = Settings.bPaletteToTexture;
}
