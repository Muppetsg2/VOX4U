// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "VoxNameFormatter.h"
#include "VoxAssetUtils.h"
#include "VoxImportOption.h"
#include "VoxScene.h"

class UMaterial;
class UMaterialInterface;
class UTexture2D;

/** MagicaVoxel material expressed with Unreal's surface inputs. */
struct FVoxSurface
{
	float Roughness = 0.5f;
	float Metallic = 0.0f;
	float Specular = 0.5f;
	/** Emissive multiplier of the base colour. */
	float Emission = 0.0f;
	float Opacity = 1.0f;
	float IOR = 1.0f;
	bool bTranslucent = false;

	static FVoxSurface FromMaterial(const v4c_material& Material);
};

/**
 * Creates (or updates, on reimport) the materials and textures of one import.
 * Shared assets are created on first use, so imports that need no materials create none.
 */
class FVoxMaterialBuilder
{
public:

	FVoxMaterialBuilder(const FVoxScene& InScene, const FVoxImportSettings& InSettings, const FVoxAssetLocation& InLocation);

	/** Material for a scene material, nullptr when materials are not imported (the engine default is used). */
	UMaterialInterface* GetMaterial(uint32 Material);

	/** Default value of the EmissionScale material parameter. */
	static constexpr float DefaultEmissionScale = 4.0f;

private:

	UTexture2D* GetPaletteTexture();
	UTexture2D* GetPropertiesTexture();
	UMaterial* GetOneMaterial();
	UMaterial* GetParentMaterial();
	UMaterialInterface* CreateMaterialInstance(uint32 Material);

	FString AssetName(EVoxAssetType ObjectType, int32 Color, const FString& Description) const;

	const FVoxScene& Scene;
	FVoxImportSettings Settings;
	FVoxAssetLocation Location;

	UTexture2D* PaletteTexture = nullptr;
	UTexture2D* PropertiesTexture = nullptr;
	UMaterial* SharedMaterial = nullptr;
	bool bSharedMaterialDone = false;
	TMap<uint32, UMaterialInterface*> Instances;
};
