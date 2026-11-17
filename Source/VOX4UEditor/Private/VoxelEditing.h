// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

class UMaterialInterface;
class UVoxel;

/** What a left click in the voxel editor's viewport does. */
enum class EVoxelEditTool : uint8
{
	/** Clicks only move the camera. */
	Select,
	/** Adds a voxel on the clicked face (Shift: removes). */
	Add,
	/** Removes the clicked voxel. */
	Remove,
	/** Paints the clicked voxel with the selected colour (Shift: picks its colour). */
	Paint,
};

/** State of the voxel editor shared by its viewport and its tools panel. */
struct FVoxelEditState
{
	EVoxelEditTool Tool = EVoxelEditTool::Select;

	/** Selected colour: index into UVoxel::Materials. */
	int32 Colour = 0;

	/** Broadcast when Tool or Colour change. */
	FSimpleMulticastDelegate OnChanged;
};

/** Result of a ray cast against a voxel asset. */
struct FVoxelRayHit
{
	/** The ray hit the voxel HitCell. */
	bool bHitVoxel = false;
	FIntVector HitCell = FIntVector::ZeroValue;

	/** A voxel can be added at AddCell: next to the hit face, or on the floor of the grid. */
	bool bCanAdd = false;
	FIntVector AddCell = FIntVector::ZeroValue;
};

/**
 * Edits of voxel assets. Grid helpers and edit steps work on the asset only; ApplyEdit wraps them in an undoable
 * transaction and updates every component that shows the asset.
 */
namespace VoxelEditing
{
	/** Colours are stored in a byte. */
	constexpr int32 MaxColours = 256;

	/**
	 * Casts a ray given in the space of the voxel component (Direction need not be normalized). Hits the first voxel
	 * on the ray; if there is none, the floor of the grid (the bottom of the lowest voxels, or z = 0 when empty),
	 * so that voxels can be added next to the model and to an empty asset.
	 */
	bool Raycast(const UVoxel& Voxel, const FVector& Start, const FVector& Direction, FVoxelRayHit& OutHit);

	/** Edit steps; each returns true if the asset changed. */
	bool AddVoxel(UVoxel& Voxel, const FIntVector& Cell, int32 Colour);
	bool RemoveVoxel(UVoxel& Voxel, const FIntVector& Cell);
	bool PaintVoxel(UVoxel& Voxel, const FIntVector& Cell, int32 Colour);
	/** Adds a colour (drawn with the asset's Mesh); returns its index, INDEX_NONE when there are MaxColours. */
	int32 AddColour(UVoxel& Voxel, UMaterialInterface* Material);
	/** Removes a colour and its voxels; the colours after it move down by one. The last colour cannot be removed. */
	bool RemoveColour(UVoxel& Voxel, int32 Colour);
	bool SetColourMaterial(UVoxel& Voxel, int32 Colour, UMaterialInterface* Material);

	/**
	 * Keeps the voxels where they are while the grid changes: XY Center becomes the same pivot, cells below zero
	 * are moved to zero together with the pivot, and Size fits the voxels.
	 */
	void Normalize(UVoxel& Voxel);

	/** Number of voxels of a colour. */
	int32 CountVoxels(const UVoxel& Voxel, int32 Colour);

	/**
	 * Runs an edit step as one undoable transaction, then normalizes the grid and rebuilds the components that
	 * show the asset. Returns the result of the step.
	 */
	bool ApplyEdit(UVoxel& Voxel, const FText& Description, TFunctionRef<bool(UVoxel&)> Edit);
}
