// Copyright 2016-2018 mik14a / Admix Network. All Rights Reserved.
// Edited by Muppetsg2 2025-2026

#pragma once

#include "CoreMinimal.h"
#include "EditorFramework/AssetImportData.h"
#include "VoxImportOption.h"
#include "VoxAssetImportData.generated.h"

/**
 * Asset import data: everything needed to reimport the asset in place.
 */
UCLASS()
class UVoxAssetImportData : public UAssetImportData
{
	GENERATED_BODY()

public:

	UPROPERTY(VisibleAnywhere, Category = "General", Meta = (DisplayName = "Import Type", ToolTip = "How the asset was imported."))
	EVoxImportType VoxImportType;

	UPROPERTY(VisibleAnywhere, Category = "General", Meta = (DisplayName = "Base Name", ToolTip = "Base name the asset names were generated from; materials and textures are found again by it on reimport."))
	FString BaseName;

	UPROPERTY(VisibleAnywhere, Category = "General", Meta = (DisplayName = "Model Index", ToolTip = "Index of the model inside the .vox file, -1 for merged models."))
	int32 ModelIndex;

	UPROPERTY(VisibleAnywhere, Category = "General", Meta = (DisplayName = "Animation Frame", ToolTip = "Animation frame baked into this static mesh, -1 if it is not a frame of an animation."))
	int32 AnimationFrame;

	UPROPERTY(EditAnywhere, Category = "General", Meta = (DisplayName = "Naming Convention", ToolTip = "Name patterns of the created assets, set in Project Settings > Plugins > VOX4U."))
	EVoxNamingConvention AssetsNamingConvention;

	UPROPERTY(EditAnywhere, Category = "General", Meta = (DisplayName = "Resources Location", ToolTip = "Where materials, textures and voxel cubes are saved."))
	EVoxResourcesSaveLocation ResourcesSaveLocation;

	UPROPERTY(EditAnywhere, Category = "General", Meta = (DisplayName = "Import X Forward", ToolTip = "Turn the front of the model (MagicaVoxel -Y) towards Unreal +X. Off keeps it facing -Y."))
	uint32 bImportXForward : 1;

	UPROPERTY(EditAnywhere, Category = "General", Meta = (DisplayName = "Pivot", ToolTip = "Origin of the generated meshes."))
	EVoxPivot Pivot;

	UPROPERTY(EditAnywhere, Category = "General", Meta = (DisplayName = "Import Hidden", ToolTip = "Also import nodes hidden in MagicaVoxel (hidden node, group or layer)."))
	uint32 bImportHidden : 1;

	UPROPERTY(EditAnywhere, Category = "General", Meta = (DisplayName = "Frame", ClampMin = "0", ToolTip = "Animation frame used to place the models of merged assets."))
	int32 Frame;

	UPROPERTY(EditAnywhere, Category = "General", Meta = (DisplayName = "Scale", ClampMin = "0.001", ToolTip = "Size of one voxel in Unreal units (cm)."))
	float Scale;

	UPROPERTY(EditAnywhere, Category = "General", Meta = (DisplayName = "Generate Lightmap UVs", ToolTip = "Generate lightmap UVs (channel 2) from the planar voxel UVs (channel 1)."))
	uint32 bGenerateLightmapUVs : 1;

	UPROPERTY(EditAnywhere, Category = "Materials", Meta = (DisplayName = "Import Materials", ToolTip = "Create materials and textures. Off uses the engine's default material."))
	uint32 bImportMaterial : 1;

	UPROPERTY(EditAnywhere, Category = "Materials", Meta = (DisplayName = "One Material", ToolTip = "One opaque material for everything, with palette and properties textures."))
	uint32 bOneMaterial : 1;

	UPROPERTY(EditAnywhere, Category = "Materials", Meta = (DisplayName = "Palette To Texture", ToolTip = "Material instances take their color from the palette texture instead of a Color parameter."))
	uint32 bPaletteToTexture : 1;

public:

	UVoxAssetImportData();

	FVoxImportSettings ToSettings() const;

	void FromSettings(const FVoxImportSettings& Settings, const FString& InBaseName, int32 InModelIndex, int32 InAnimationFrame);
};
