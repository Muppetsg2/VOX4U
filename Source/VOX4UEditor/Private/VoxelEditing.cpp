// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#include "VoxelEditing.h"
#include "ScopedTransaction.h"
#include "Voxel.h"

namespace VoxelEditingPrivate
{
	constexpr double Huge = 1.0e30;
	constexpr double Tiny = 1.0e-12;

	int32 Floor(double Value)
	{
		return static_cast<int32>(FMath::FloorToDouble(Value));
	}

	/** Smallest and largest cell of the voxels; false if there are none. */
	bool GetCellRange(const UVoxel& Voxel, FIntVector& OutMin, FIntVector& OutMax)
	{
		if (Voxel.Voxels.Num() == 0)
		{
			return false;
		}
		OutMin = FIntVector(TNumericLimits<int32>::Max());
		OutMax = FIntVector(TNumericLimits<int32>::Lowest());
		for (const TPair<FIntVector, uint8>& Cell : Voxel.Voxels)
		{
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				OutMin[Axis] = FMath::Min(OutMin[Axis], Cell.Key[Axis]);
				OutMax[Axis] = FMath::Max(OutMax[Axis], Cell.Key[Axis]);
			}
		}
		return true;
	}
}

bool VoxelEditing::Raycast(const UVoxel& Voxel, const FVector& Start, const FVector& Direction, FVoxelRayHit& OutHit)
{
	using namespace VoxelEditingPrivate;

	OutHit = FVoxelRayHit();

	const FVector CellSize = Voxel.GetCellSize();
	if (CellSize.X <= 0.0 || CellSize.Y <= 0.0 || CellSize.Z <= 0.0)
	{
		return false;
	}

	// Grid space: cell C is the unit box [C, C + 1].
	const FVector Origin = (Start + Voxel.GetGridOffset()) / CellSize;
	const FVector Ray = Direction / CellSize;
	if (Ray.IsNearlyZero(Tiny))
	{
		return false;
	}

	FIntVector Min, Max;
	const bool bHasVoxels = GetCellRange(Voxel, Min, Max);
	if (bHasVoxels)
	{
		// Part of the ray inside the box of all voxels.
		double Enter = -Huge;
		double Exit = Huge;
		int32 EnterAxis = INDEX_NONE;
		bool bMisses = false;
		for (int32 Axis = 0; Axis < 3 && !bMisses; ++Axis)
		{
			const double Low = Min[Axis];
			const double High = Max[Axis] + 1.0;
			if (FMath::Abs(Ray[Axis]) < Tiny)
			{
				bMisses = Origin[Axis] < Low || Origin[Axis] > High;
				continue;
			}
			double Near = (Low - Origin[Axis]) / Ray[Axis];
			double Far = (High - Origin[Axis]) / Ray[Axis];
			if (Near > Far)
			{
				Swap(Near, Far);
			}
			if (Near > Enter)
			{
				Enter = Near;
				EnterAxis = Axis;
			}
			Exit = FMath::Min(Exit, Far);
		}

		if (!bMisses && Enter <= Exit && Exit >= 0.0)
		{
			// Walk the cells along the ray (Amanatides & Woo).
			const double T = FMath::Max(Enter, 0.0);
			const FVector Point = Origin + Ray * T;
			FIntVector Cell;
			FIntVector Step;
			FVector NextBoundary;
			FVector BoundaryStep;
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				Cell[Axis] = FMath::Clamp(Floor(Point[Axis]), Min[Axis], Max[Axis]);
				Step[Axis] = Ray[Axis] > Tiny ? 1 : (Ray[Axis] < -Tiny ? -1 : 0);
				NextBoundary[Axis] = Step[Axis] == 0 ? Huge : ((Cell[Axis] + (Step[Axis] > 0 ? 1.0 : 0.0)) - Origin[Axis]) / Ray[Axis];
				BoundaryStep[Axis] = Step[Axis] == 0 ? Huge : FMath::Abs(1.0 / Ray[Axis]);
			}

			// Face the ray came through; none if it starts inside the box.
			FIntVector Normal = FIntVector::ZeroValue;
			if (Enter >= 0.0 && EnterAxis != INDEX_NONE)
			{
				Normal[EnterAxis] = -Step[EnterAxis];
			}

			const FIntVector Extent = Max - Min;
			const int32 MaxSteps = Extent.X + Extent.Y + Extent.Z + 3;
			for (int32 Index = 0; Index <= MaxSteps; ++Index)
			{
				if (Cell.X < Min.X || Cell.Y < Min.Y || Cell.Z < Min.Z || Cell.X > Max.X || Cell.Y > Max.Y || Cell.Z > Max.Z)
				{
					break;
				}
				if (Voxel.Voxels.Contains(Cell))
				{
					OutHit.bHitVoxel = true;
					OutHit.HitCell = Cell;
					OutHit.bCanAdd = Normal != FIntVector::ZeroValue;
					OutHit.AddCell = Cell + Normal;
					return true;
				}
				int32 Axis = NextBoundary.X < NextBoundary.Y ? 0 : 1;
				Axis = NextBoundary[Axis] < NextBoundary.Z ? Axis : 2;
				Cell[Axis] += Step[Axis];
				NextBoundary[Axis] += BoundaryStep[Axis];
				Normal = FIntVector::ZeroValue;
				Normal[Axis] = -Step[Axis];
			}
		}
	}

	// No voxel: the floor of the grid, seen from above.
	const int32 FloorZ = bHasVoxels ? Min.Z : 0;
	if (Ray.Z < -Tiny && Origin.Z > FloorZ)
	{
		const double T = (FloorZ - Origin.Z) / Ray.Z;
		const FVector Point = Origin + Ray * T;
		OutHit.bCanAdd = true;
		OutHit.AddCell = FIntVector(Floor(Point.X), Floor(Point.Y), FloorZ);
		return true;
	}
	return false;
}

bool VoxelEditing::AddVoxel(UVoxel& Voxel, const FIntVector& Cell, int32 Colour)
{
	if (Colour < 0 || Colour >= Voxel.GetNumColours() || Voxel.Voxels.Contains(Cell))
	{
		return false;
	}
	Voxel.Voxels.Add(Cell, static_cast<uint8>(Colour));
	return true;
}

bool VoxelEditing::RemoveVoxel(UVoxel& Voxel, const FIntVector& Cell)
{
	return Voxel.Voxels.Remove(Cell) > 0;
}

bool VoxelEditing::PaintVoxel(UVoxel& Voxel, const FIntVector& Cell, int32 Colour)
{
	uint8* Value = Voxel.Voxels.Find(Cell);
	if (!Value || Colour < 0 || Colour >= Voxel.GetNumColours() || *Value == Colour)
	{
		return false;
	}
	*Value = static_cast<uint8>(Colour);
	return true;
}

int32 VoxelEditing::AddColour(UVoxel& Voxel, UMaterialInterface* Material)
{
	const int32 NumColours = Voxel.GetNumColours();
	if (NumColours >= MaxColours)
	{
		return INDEX_NONE;
	}
	Voxel.Materials.SetNum(NumColours);
	Voxel.Materials.Add(Material);
	return NumColours;
}

bool VoxelEditing::RemoveColour(UVoxel& Voxel, int32 Colour)
{
	if (Colour < 0 || Colour >= Voxel.GetNumColours() || Voxel.GetNumColours() <= 1)
	{
		return false;
	}

	TMap<FIntVector, uint8> Kept;
	Kept.Reserve(Voxel.Voxels.Num());
	for (const TPair<FIntVector, uint8>& Cell : Voxel.Voxels)
	{
		if (Cell.Value != Colour)
		{
			Kept.Add(Cell.Key, Cell.Value > Colour ? static_cast<uint8>(Cell.Value - 1) : Cell.Value);
		}
	}
	Voxel.Voxels = MoveTemp(Kept);

	if (Voxel.Materials.IsValidIndex(Colour))
	{
		Voxel.Materials.RemoveAt(Colour);
	}
	if (Voxel.Meshes.IsValidIndex(Colour))
	{
		Voxel.Meshes.RemoveAt(Colour); // own cube of an asset imported before 0.6.0
	}
#if WITH_EDITOR
	Voxel.CalcCellBounds();
#endif
	return true;
}

bool VoxelEditing::SetColourMaterial(UVoxel& Voxel, int32 Colour, UMaterialInterface* Material)
{
	if (Colour < 0 || Colour >= Voxel.GetNumColours() || Voxel.GetMaterial(Colour) == Material)
	{
		return false;
	}
	Voxel.Materials.SetNum(Voxel.GetNumColours());
	Voxel.Materials[Colour] = Material;
	return true;
}

void VoxelEditing::Normalize(UVoxel& Voxel)
{
	using namespace VoxelEditingPrivate;

	// XY Center moves the voxels whenever the size changes: use the pivot it gives for the current size instead.
	if (!Voxel.bUsePivot)
	{
		Voxel.Pivot = Voxel.bXYCenter ? FVector(Voxel.Size.X * 0.5, Voxel.Size.Y * 0.5, 0.0) : FVector::ZeroVector;
		Voxel.bUsePivot = true;
	}

	FIntVector Min, Max;
	if (!GetCellRange(Voxel, Min, Max))
	{
		Voxel.Size = FIntVector::ZeroValue;
		return;
	}

	// Cells start at zero, as in imported assets; the pivot moves with them.
	const FIntVector Shift(FMath::Min(Min.X, 0), FMath::Min(Min.Y, 0), FMath::Min(Min.Z, 0));
	if (Shift != FIntVector::ZeroValue)
	{
		TMap<FIntVector, uint8> Moved;
		Moved.Reserve(Voxel.Voxels.Num());
		for (const TPair<FIntVector, uint8>& Cell : Voxel.Voxels)
		{
			Moved.Add(Cell.Key - Shift, Cell.Value);
		}
		Voxel.Voxels = MoveTemp(Moved);
		Voxel.Pivot -= FVector(Shift);
	}
	Voxel.Size = Max - Shift + FIntVector(1, 1, 1);
}

int32 VoxelEditing::CountVoxels(const UVoxel& Voxel, int32 Colour)
{
	int32 Count = 0;
	for (const TPair<FIntVector, uint8>& Cell : Voxel.Voxels)
	{
		Count += Cell.Value == Colour ? 1 : 0;
	}
	return Count;
}

bool VoxelEditing::ApplyEdit(UVoxel& Voxel, const FText& Description, TFunctionRef<bool(UVoxel&)> Edit)
{
	FScopedTransaction Transaction(Description);
	Voxel.SetFlags(RF_Transactional);
	Voxel.Modify();
	if (!Edit(Voxel))
	{
		Transaction.Cancel();
		return false;
	}
	Normalize(Voxel);
	Voxel.PostEditChange(); // rebuilds the components (UVoxel::RefreshComponents) and refreshes the editor
	return true;
}
