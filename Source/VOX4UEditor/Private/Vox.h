// Copyright 2016-2018 mik14a / Admix Network. All Rights Reserved.
// Edited by Muppetsg2 2025

#pragma once

#include <CoreMinimal.h>
#include <RawMesh.h>
#include "VoxMaterial.h"

class UTexture2D;
class UVoxImportOption;

/**
 * @struct FVoxModelData
 */
struct FVoxModelData 
{
	TMap<FIntVector, uint8> Data;
};

/**
 * @struct FVox
 * VOX format implementation.
 * @see https://github.com/ephtracy/voxel-model/blob/master/MagicaVoxel-file-format-vox.txt
 */
struct FVox
{
	/** Filename */
	FString Filename;

	/** Magic number ( 'V' 'O' 'X' 'space' ) and terminate */
	ANSICHAR MagicNumber[5];
	/** version number ( current version is 200 ) */
	uint32 VersionNumber;

	/** Size */
	TArray<FIntVector> Size;
	/** Voxel */
	TArray<FVoxModelData> Voxel;
	/** Palette */
	TArray<FColor> Palette;
	/** Materials */
	TArray<FVoxMaterial> Materials;

public:

	/** Create empty vox data */
	FVox();

	/** Create vox data from archive */
	FVox(const FString& Filename, FArchive& Ar, const UVoxImportOption* ImportOption);

	/** Import vox data from archive */
	bool Import(FArchive& Ar, const UVoxImportOption* ImportOption);

	/** Create FRawMesh from Voxel */
	bool CreateRawMesh(FRawMesh& OutRawMesh, const UVoxImportOption* ImportOption, const uint32 ModelId) const;

	/** Create FRawMesh from Voxel use Monotone mesh generation */
	bool CreateOptimizedRawMesh(FRawMesh& OutRawMesh, const UVoxImportOption* ImportOption, const uint32 ModelId) const;

	/** Create raw meshes from Voxel */
	bool CreateRawMeshes(TArray<FRawMesh>& OutRawMeshes, const UVoxImportOption* ImportOption, const uint32 ModelId) const;

	/** Create UTexture2D from Palette */
	bool CreateTexture(UTexture2D* const& OutTexture, UVoxImportOption* ImportOption) const;

	/** Get Unique Colors from Palette */
	void GetUniqueColors(TArray<uint8>& OutPalette) const;

	/** Get Unique Colors from Model Palette */
	void GetUniqueColors(TArray<uint8>& OutPalette, const uint32 ModelId) const;

	/** Get Biggest Size */
	void GetBiggestSize(FIntVector& OutSize) const;

	/** Create one raw mesh */
	static bool CreateMesh(FRawMesh& OutRawMesh, const UVoxImportOption* ImportOption);
};