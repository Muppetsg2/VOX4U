// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Templates/UniquePtr.h"
#include "VoxAssetUtils.h"
#include "VoxImportOption.h"
#include "VoxNameFormatter.h"
#include "VoxScene.h"

class FVoxMaterialBuilder;
class UBlueprint;
class UStaticMesh;
class UVoxel;
class UVoxelAnimation;
struct FVoxelAnimationTrack;

/**
 * Turns a .vox file into assets. Used for imports (all assets of a file) and reimports (one asset in place).
 */
class FVoxImporter
{
public:

	FVoxImporter(const FVoxImportSettings& InSettings, const FVoxAssetLocation& InLocation, const FString& InSourceFile);
	~FVoxImporter();

	/** Creates or updates every asset of the file. The main asset comes first. */
	TArray<UObject*> Import(const uint8* Data, int64 Size);

	/**
	 * Rebuilds an asset made by an earlier import. ModelIndex is the model of the file (-1 for merged models),
	 * AnimationFrame the frame baked into a static mesh of an animation (-1 otherwise).
	 */
	bool Reimport(UObject* Asset, int32 ModelIndex, int32 AnimationFrame, const uint8* Data, int64 Size);

	/** Reason of the last failure. */
	const FText& GetError() const { return Error; }

private:

	bool LoadScene(const uint8* Data, int64 Size, bool bMeshes, EVoxPivot Pivot);

	TArray<UObject*> ImportStaticMeshes();
	TArray<UObject*> ImportAnimationFrames();
	TArray<UObject*> ImportVoxels();
	TArray<UObject*> ImportScene();

	/** Model with the given index in the file, nullptr if there is none. */
	const v4c_model* FindModel(int32 FileIndex) const;

	/** Name of an asset; BaseName defaults to the base name of the import. */
	FString AssetName(EVoxAssetType Type, int32 ModelIndex, int32 Color = INDEX_NONE, const FString& BaseName = FString()) const;

	/** Base name of the static mesh of an animation frame (Name_F12). */
	FString FrameBaseName(uint32 Frame) const;

	/** True if animation assets are created for this import. */
	bool ShouldImportAnimation() const;

	/**
	 * Frames of the animation range that look different. OutFrameOf maps every frame of the range to the
	 * first frame with the same models and placement.
	 */
	void GetDistinctFrames(TArray<uint32>& OutFrames, TMap<uint32, uint32>& OutFrameOf) const;

	/** Origin shared by the static meshes of all animation frames, so that they do not jump. */
	FVector3f GetAnimationPivotOffset();

	/**
	 * ModelIndex >= 0: that model on its own; -1: every (visible) model placed as in the scene, at
	 * AnimationFrame if given, else at Settings.Frame.
	 */
	bool BuildStaticMesh(UStaticMesh* StaticMesh, int32 ModelIndex, int32 AnimationFrame = INDEX_NONE);
	bool BuildVoxel(UVoxel* Voxel, int32 ModelIndex);
	/** The cube of the voxel assets of this import (shared by all of them); Material sets its default material. */
	UStaticMesh* BuildVoxelCube(uint32 Material);

	/** Voxel assets take their colours from material instances, never from the palette texture of the cube. */
	void UseColourMaterials();

	/** Creates or updates the Voxel Animation asset of the import. */
	UVoxelAnimation* WriteAnimation(TArray<UObject*> Models, TArray<FVoxelAnimationTrack> Tracks, bool bInterpolate);

	/** Rebuilds the components of the scene Blueprint; returns the animation it plays, if any. */
	UVoxelAnimation* BuildBlueprint(UBlueprint* Blueprint, const TMap<int32, UObject*>& Models, uint32 Frame);

	template <class TAsset>
	void UpdateImportData(TAsset* Asset, int32 ModelIndex, int32 AnimationFrame, const FVoxImportSettings& AssetSettings) const;

	FVoxImportSettings Settings;
	FVoxAssetLocation Location;
	FString SourceFile;
	FText Error;

	FVoxScene Scene;
	TUniquePtr<FVoxMaterialBuilder> Materials;

	/** Cube built by BuildVoxelCube for this import. */
	UStaticMesh* VoxelCube = nullptr;

	bool bAnimationPivotValid = false;
	FVector3f AnimationPivotOffset = FVector3f::ZeroVector;
};
