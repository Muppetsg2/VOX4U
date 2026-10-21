// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "MeshDescription.h"
#include "Templates/Function.h"
#include "VoxScene.h"

class UMaterialInterface;
class UStaticMesh;

/** Mesh ready to be written into a static mesh. */
struct FVoxMeshData
{
	FMeshDescription Description;
	/** Scene material of every polygon group / material slot, in slot order. */
	TArray<uint32> Materials;
	int32 NumTriangles = 0;
};

namespace VoxMeshBuilder
{
	/**
	 * Converts a vox4c mesh (the mesh of a model, or a frame baked by FVoxScene::BakeFrame) moved by -Offset.
	 * UV0 samples the palette (texel centre of the material), UV1 is a planar projection in voxel units,
	 * vertex colours hold the palette colour. Positions shared by faces are welded, every edge is hard.
	 * The mesh must be wound clockwise (FVoxScene loads and bakes it that way).
	 */
	bool BuildMesh(const FVoxScene& Scene, const v4c_mesh& Mesh, const FVector3f& Offset, FVoxMeshData& Out);

	/** Bounds of the vertices of a vox4c mesh. */
	FBox3f GetBounds(const v4c_mesh& Mesh);

	/** Point of Bounds that becomes the origin for a pivot (zero for EVoxPivot::MagicaVoxel). */
	FVector3f GetPivotOffset(const FBox3f& Bounds, EVoxPivot Pivot);

	/** A cube of Size units centred at the origin, using one material. */
	void BuildCube(const FVoxScene& Scene, uint32 Material, float Size, FVoxMeshData& Out);

	/**
	 * Replaces LOD 0 of StaticMesh (new or existing asset) with Data, assigns the materials and builds it.
	 */
	void CommitStaticMesh(UStaticMesh* StaticMesh, FVoxMeshData& Data, const FVoxScene& Scene,
		TFunctionRef<UMaterialInterface*(uint32 Material)> GetMaterial, bool bGenerateLightmapUVs);
}
