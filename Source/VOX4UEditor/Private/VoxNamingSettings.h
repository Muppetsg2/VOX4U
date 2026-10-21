// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "VoxImportOption.h"
#include "VoxNamingSettings.generated.h"

/** Kind of asset a name is generated for. */
enum class EVoxAssetType : uint8
{
	Material,
	MaterialInstance,
	Texture,
	StaticMesh,
	Voxel,
	Blueprint,
	Animation
};

/** Name patterns of one naming convention. */
USTRUCT()
struct FVoxNamingPatterns
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Patterns", Meta = (DisplayName = "Material", ToolTip = "Parent material (one material mode, or the parent of the material instances)."))
	FString Material;

	UPROPERTY(EditAnywhere, Category = "Patterns", Meta = (DisplayName = "Material Instance", ToolTip = "Material instance of one colour. {Color} is its palette index."))
	FString MaterialInstance;

	UPROPERTY(EditAnywhere, Category = "Patterns", Meta = (DisplayName = "Texture", ToolTip = "Lookup textures. {Description} is Palette or Properties."))
	FString Texture;

	UPROPERTY(EditAnywhere, Category = "Patterns", Meta = (DisplayName = "Static Mesh", ToolTip = "Static meshes. {ModelId} is the model index of separate models, {Color} the palette index of a voxel cube."))
	FString StaticMesh;

	UPROPERTY(EditAnywhere, Category = "Patterns", Meta = (DisplayName = "Voxel", ToolTip = "Voxel assets. {ModelId} is the model index of separate models."))
	FString Voxel;

	UPROPERTY(EditAnywhere, Category = "Patterns", Meta = (DisplayName = "Blueprint", ToolTip = "Scene Blueprint."))
	FString Blueprint;

	UPROPERTY(EditAnywhere, Category = "Patterns", Meta = (DisplayName = "Animation", ToolTip = "Voxel Animation asset of an animated file."))
	FString Animation;

	/** Pattern of an asset type. */
	const FString& Get(EVoxAssetType Type) const;

	/** Built-in patterns of a convention. */
	static FVoxNamingPatterns MakeDefault(EVoxNamingConvention Convention);
};

/**
 * Asset name patterns, edited in Project Settings > Plugins > VOX4U and saved in Config/DefaultEditor.ini.
 *
 * Tokens: {BaseName}, {ModelId}, {Color}, {Description}. An underscore inside the braces ({_Color}, {Color_},
 * {_Color_}) is only written when the token has a value, so "SM_{BaseName}{_ModelId}" gives SM_Tree for a
 * merged mesh and SM_Tree_2 for the third model.
 */
UCLASS(config = Editor, defaultconfig)
class UVoxNamingSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	UVoxNamingSettings();

	UPROPERTY(EditAnywhere, config, Category = "Asset Names", Meta = (DisplayName = "Unreal Engine Convention", ToolTip = "Patterns used with the Unreal Engine naming convention (prefixes such as SM_, MI_, T_). Tokens: {BaseName}, {ModelId}, {Color}, {Description}; underscores inside the braces are only written when the token has a value."))
	FVoxNamingPatterns UnrealEngineNames;

	UPROPERTY(EditAnywhere, config, Category = "Asset Names", Meta = (DisplayName = "VOX4U Convention", ToolTip = "Patterns used with the VOX4U naming convention (suffixes such as _MT, _MI, _TX). Tokens: {BaseName}, {ModelId}, {Color}, {Description}; underscores inside the braces are only written when the token has a value."))
	FVoxNamingPatterns VOX4UNames;

	const FVoxNamingPatterns& GetPatterns(EVoxNamingConvention Convention) const;

	virtual FName GetCategoryName() const override;

#if WITH_EDITOR
	virtual FText GetSectionText() const override;
	virtual FText GetSectionDescription() const override;
#endif
};
