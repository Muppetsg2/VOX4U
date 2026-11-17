// Copyright 2016-2018 mik14a / Admix Network. All Rights Reserved.
// Edited by Muppetsg2 2025-2026

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "VoxImportOption.generated.h"

// Every DisplayName / ToolTip below is translated through Localization/<culture>.po
// (keys "UObjectDisplayNames,<Class>:<Property>" and "UObjectToolTips,<Class>:<Property>").

/** Import mesh type */
UENUM(BlueprintType)
enum class EVoxImportType : uint8
{
	StaticMesh UMETA(DisplayName = "Static Mesh", ToolTip = "Static meshes built with monotone meshing."),
	Voxel UMETA(DisplayName = "Voxel", ToolTip = "A voxel asset drawn with instanced cubes, one cube mesh per colour."),
	Scene UMETA(DisplayName = "Scene (Blueprint)", ToolTip = "A Blueprint actor that rebuilds the MagicaVoxel scene graph: one component per node with its transform, one shared static mesh per model.")
};

/** Assets Naming Convention */
UENUM(BlueprintType)
enum class EVoxNamingConvention : uint8
{
	UnrealEngine UMETA(DisplayName = "Unreal Engine", ToolTip = "Prefixes such as SM_, MI_, T_ (Project Settings > Plugins > VOX4U)."),
	VOX4U UMETA(DisplayName = "VOX4U", ToolTip = "Suffixes such as _MT, _MI, _TX (Project Settings > Plugins > VOX4U).")
};

/** Resources save location */
UENUM(BlueprintType)
enum class EVoxResourcesSaveLocation : uint8
{
	SubFolder UMETA(DisplayName = "In Subfolder (MeshName_Resources)", ToolTip = "Materials, textures and voxel cubes go into a folder named after the asset."),
	SameFolder UMETA(DisplayName = "In Same Folder as Mesh", ToolTip = "Materials, textures and voxel cubes go next to the asset.")
};

/** Origin of the generated meshes */
UENUM(BlueprintType)
enum class EVoxPivot : uint8
{
	MagicaVoxel UMETA(DisplayName = "MagicaVoxel", ToolTip = "Separate models: the MagicaVoxel pivot, floor(size / 2). Merged models: the origin of the MagicaVoxel scene."),
	Corner UMETA(DisplayName = "Corner", ToolTip = "Minimum corner of the bounding box."),
	Center UMETA(DisplayName = "Center", ToolTip = "Center of the bounding box."),
	BottomCenter UMETA(DisplayName = "Bottom Center", ToolTip = "Centered horizontally, standing on Z = 0.")
};

/** What the nodes of a scene Blueprint show */
UENUM(BlueprintType)
enum class EVoxSceneModels : uint8
{
	StaticMesh UMETA(DisplayName = "Static Meshes", ToolTip = "One static mesh per model (monotone meshing): few triangles, the fastest to render."),
	Voxel UMETA(DisplayName = "Voxel Assets", ToolTip = "One voxel asset per model, drawn with instanced cubes: every voxel stays a separate cube.")
};

/** How animated transforms are sampled */
UENUM(BlueprintType)
enum class EVoxAnimationSampling : uint8
{
	MagicaVoxel UMETA(DisplayName = "MagicaVoxel", ToolTip = "As MagicaVoxel plays it: every frame is held, translations stay on the voxel grid, rotations switch at their keys."),
	Smooth UMETA(DisplayName = "Smooth", ToolTip = "For playback in a game: translations and rotations are interpolated between keys and between frames.")
};

/**
 * Plain copy of the options used by the importer.
 * Filled from the import dialog (UVoxImportOption) or from the asset import data on reimport.
 */
struct FVoxImportSettings
{
	EVoxImportType ImportType = EVoxImportType::StaticMesh;
	EVoxNamingConvention NamingConvention = EVoxNamingConvention::UnrealEngine;
	EVoxResourcesSaveLocation ResourcesSaveLocation = EVoxResourcesSaveLocation::SubFolder;

	bool bImportXForward = true;
	EVoxPivot Pivot = EVoxPivot::BottomCenter;
	bool bSeparateModels = false;
	bool bImportHidden = false;
	int32 Frame = 0;
	float Scale = 1.0f;
	bool bGenerateLightmapUVs = true;

	bool bImportMaterial = true;
	bool bOneMaterial = false;
	bool bPaletteToTexture = false;

	EVoxSceneModels SceneModels = EVoxSceneModels::StaticMesh;

	bool bImportAnimation = true;
	EVoxAnimationSampling AnimationSampling = EVoxAnimationSampling::MagicaVoxel;
	float AnimationFramesPerSecond = 10.0f;

	bool bImportSun = true;
	bool bImportCameras = true;
	float LightIntensityScale = 10.0f;
};

/**
 * Import options. Shown in the import dialog and saved per project; scripts can pass an instance
 * as AssetImportTask.options (see README).
 */
UCLASS(BlueprintType, config = EditorPerProjectUserSettings, HideCategories = Object)
class UVoxImportOption : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, config, Category = "Import Type", Meta = (DisplayName = "Import Type", ToolTip = "What the .vox file becomes: static meshes, voxel assets or a scene Blueprint."))
	EVoxImportType VoxImportType;

	UPROPERTY(EditAnywhere, config, Category = "Asset Names", Meta = (DisplayName = "Naming Convention", ToolTip = "Name patterns of the created assets, set in Project Settings > Plugins > VOX4U."))
	EVoxNamingConvention AssetsNamingConvention;

	UPROPERTY(EditAnywhere, Category = "Asset Names", Meta = (DisplayName = "Custom Asset Name", ToolTip = "Base name of the created assets. Empty: the name of the file."))
	FString CustomAssetName;

	UPROPERTY(EditAnywhere, config, Category = "General", Meta = (DisplayName = "Resources Location", ToolTip = "Where materials, textures and voxel cubes are saved."))
	EVoxResourcesSaveLocation ResourcesSaveLocation;

	UPROPERTY(EditAnywhere, config, Category = "General", Meta = (DisplayName = "Import X Forward", ToolTip = "Turn the front of the model (MagicaVoxel -Y) towards Unreal +X. Off keeps it facing -Y."))
	uint32 bImportXForward : 1;

	UPROPERTY(EditAnywhere, config, Category = "General", Meta = (DisplayName = "Pivot", EditCondition = "VoxImportType != EVoxImportType::Scene", EditConditionHides, ToolTip = "Origin of the generated meshes. A scene always uses the MagicaVoxel pivots so that the node transforms match."))
	EVoxPivot Pivot;

	UPROPERTY(EditAnywhere, config, Category = "General", Meta = (DisplayName = "Separate Models", EditCondition = "VoxImportType != EVoxImportType::Scene", EditConditionHides, ToolTip = "One asset per model of the file. Off merges the models into one asset, placed as in the MagicaVoxel scene."))
	uint32 bSeparateModels : 1;

	UPROPERTY(EditAnywhere, config, Category = "General", Meta = (DisplayName = "Import Hidden", ToolTip = "Also import nodes hidden in MagicaVoxel (hidden node, group or layer). Separate models always contain every model of the file."))
	uint32 bImportHidden : 1;

	UPROPERTY(EditAnywhere, config, Category = "General", Meta = (DisplayName = "Frame", ClampMin = "0", UIMin = "0", ToolTip = "Animation frame used to place the models of merged assets and scenes. Clamped to the last frame of the file."))
	int32 Frame;

	UPROPERTY(EditAnywhere, config, Category = "General", Meta = (DisplayName = "Scale", ClampMin = "0.001", UIMin = "0.001", ToolTip = "Size of one voxel in Unreal units (cm)."))
	float Scale;

	UPROPERTY(EditAnywhere, config, Category = "General", Meta = (DisplayName = "Generate Lightmap UVs", EditCondition = "VoxImportType != EVoxImportType::Voxel", EditConditionHides, ToolTip = "Generate lightmap UVs (channel 2) from the planar voxel UVs (channel 1)."))
	uint32 bGenerateLightmapUVs : 1;

	UPROPERTY(EditAnywhere, config, Category = "Materials", Meta = (DisplayName = "Import Materials", ToolTip = "Create materials and textures. Off uses the engine's default material."))
	uint32 bImportMaterial : 1;

	UPROPERTY(EditAnywhere, config, Category = "Materials", Meta = (DisplayName = "One Material", EditCondition = "bImportMaterial && VoxImportType != EVoxImportType::Voxel && !(VoxImportType == EVoxImportType::Scene && SceneModels == EVoxSceneModels::Voxel)", EditConditionHides, ToolTip = "Static meshes only: one opaque material for everything. Colors come from a palette texture, roughness / metallic / specular / emission from a properties texture. Voxel assets always use one material instance per color."))
	uint32 bOneMaterial : 1;

	UPROPERTY(EditAnywhere, config, Category = "Materials", Meta = (DisplayName = "Palette To Texture", EditCondition = "bImportMaterial && !bOneMaterial && VoxImportType != EVoxImportType::Voxel && !(VoxImportType == EVoxImportType::Scene && SceneModels == EVoxSceneModels::Voxel)", EditConditionHides, ToolTip = "Static meshes only: material instances take their color from the palette texture instead of a Color parameter. Voxel assets always use a Color parameter."))
	uint32 bPaletteToTexture : 1;

	UPROPERTY(EditAnywhere, config, Category = "Animation", Meta = (DisplayName = "Import Animation", EditCondition = "VoxImportType == EVoxImportType::Scene || (VoxImportType == EVoxImportType::StaticMesh && !bSeparateModels)", EditConditionHides, ToolTip = "Animated files only. Scene: a Voxel Animation asset and a component that plays it in the Blueprint. Merged static mesh: one static mesh per frame of the animation and a Voxel Animation asset that switches between them."))
	uint32 bImportAnimation : 1;

	UPROPERTY(EditAnywhere, config, Category = "Animation", Meta = (DisplayName = "Sampling", EditCondition = "bImportAnimation && VoxImportType == EVoxImportType::Scene", EditConditionHides, ToolTip = "How animated transforms are sampled."))
	EVoxAnimationSampling AnimationSampling;

	UPROPERTY(EditAnywhere, config, Category = "Animation", Meta = (DisplayName = "Frames Per Second", EditCondition = "bImportAnimation && VoxImportType != EVoxImportType::Voxel", EditConditionHides, ClampMin = "0.01", UIMin = "1", UIMax = "60", ToolTip = "Playback speed of the animation. MagicaVoxel does not store it in the file."))
	float AnimationFramesPerSecond;

	UPROPERTY(EditAnywhere, config, Category = "Scene", Meta = (DisplayName = "Scene Models", EditCondition = "VoxImportType == EVoxImportType::Scene", EditConditionHides, ToolTip = "What the nodes of the scene show: static meshes or voxel assets."))
	EVoxSceneModels SceneModels;

	UPROPERTY(EditAnywhere, config, Category = "Scene", Meta = (DisplayName = "Import Sun", EditCondition = "VoxImportType == EVoxImportType::Scene", EditConditionHides, ToolTip = "Add the MagicaVoxel sun as a directional light."))
	uint32 bImportSun : 1;

	UPROPERTY(EditAnywhere, config, Category = "Scene", Meta = (DisplayName = "Import Cameras", EditCondition = "VoxImportType == EVoxImportType::Scene", EditConditionHides, ToolTip = "Add a camera component for every camera slot a view was saved into."))
	uint32 bImportCameras : 1;

	UPROPERTY(EditAnywhere, config, Category = "Scene", Meta = (DisplayName = "Light Intensity Scale", EditCondition = "VoxImportType == EVoxImportType::Scene && bImportSun", EditConditionHides, ClampMin = "0", ToolTip = "Directional light intensity (lux) = MagicaVoxel sun intensity x this value."))
	float LightIntensityScale;

public:

	UVoxImportOption();

	/** Shows the import dialog. Returns false when the import was canceled. */
	bool GetImportOption(bool& bOutImportAll);

	/** Copy of the current options. */
	FVoxImportSettings ToSettings() const;
};
