// Copyright 2016-2018 mik14a / Admix Network. All Rights Reserved.
// Edited by Muppetsg2 2025-2026

#include "Voxel.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "VoxelComponent.h"
#if WITH_EDITOR
#include "UObject/UObjectIterator.h"
#endif
#if WITH_EDITORONLY_DATA
#include "EditorFramework/AssetImportData.h"
#endif

UVoxel::UVoxel()
	: Size(ForceInit)
	, CellBounds(FVector::ZeroVector, FVector(100.f, 100.f, 100.f), 100.f)
	, Scale(0.f) // not set: assets made before Scale existed, see PostLoad
	, bXYCenter(true)
	, bUsePivot(false)
	, Pivot(FVector::ZeroVector)
	, Mesh(nullptr)
	, Materials()
	, Meshes()
	, Voxels()
{
}

void UVoxel::PostLoad()
{
	Super::PostLoad();

	// Assets imported before 0.6.0 have one cube per colour: they stay as Legacy Meshes, Mesh is their first one
	// and every colour gets a (empty) material slot.
	if (Meshes.Num() > 0)
	{
		if (!Mesh)
		{
			Mesh = Meshes[0];
		}
		if (Materials.Num() < Meshes.Num())
		{
			Materials.SetNum(Meshes.Num());
		}
	}

	// Assets saved before Scale existed: keep the size of their cubes.
	if (Scale <= 0.f)
	{
		Scale = static_cast<float>(CellBounds.BoxExtent.GetMax() * 2.0);
	}
}

double UVoxel::GetMeshScale() const
{
	const double MeshSize = CellBounds.BoxExtent.GetMax() * 2.0;
	return Scale > 0.f && MeshSize > 0.0 ? Scale / MeshSize : 1.0;
}

FVector UVoxel::GetCellSize() const
{
	return CellBounds.BoxExtent * 2.0 * GetMeshScale();
}

FVector UVoxel::GetGridOffset() const
{
	if (bUsePivot)
	{
		return Pivot * GetCellSize(); // the pivot point of the grid lies at the origin
	}
	if (bXYCenter)
	{
		return FVector(static_cast<double>(Size.X), static_cast<double>(Size.Y), 0.0) * GetCellSize() * 0.5;
	}
	return FVector::ZeroVector;
}

FBox UVoxel::GetCellBox(const FIntVector& Cell) const
{
	const FVector CellSize = GetCellSize();
	const FVector Min = FVector(Cell) * CellSize - GetGridOffset();
	return FBox(Min, Min + CellSize);
}

FIntVector UVoxel::GetCellAt(const FVector& Position) const
{
	const FVector CellSize = GetCellSize();
	const FVector Grid = (Position + GetGridOffset()) / CellSize.ComponentMax(FVector(1.0e-6));
	return FIntVector(
		static_cast<int32>(FMath::FloorToDouble(Grid.X)),
		static_cast<int32>(FMath::FloorToDouble(Grid.Y)),
		static_cast<int32>(FMath::FloorToDouble(Grid.Z)));
}

int32 UVoxel::GetNumColours() const
{
	return FMath::Max(Materials.Num(), Meshes.Num());
}

UStaticMesh* UVoxel::GetColourMesh(int32 ColourIndex) const
{
	if (Meshes.IsValidIndex(ColourIndex) && Meshes[ColourIndex])
	{
		return Meshes[ColourIndex].Get();
	}
	return Mesh.Get();
}

UMaterialInterface* UVoxel::GetMaterial(int32 ColourIndex) const
{
	return Materials.IsValidIndex(ColourIndex) ? Materials[ColourIndex].Get() : nullptr;
}

#if WITH_EDITORONLY_DATA

UAssetImportData* UVoxel::GetAssetImportData() const
{
	return AssetImportData;
}

void UVoxel::SetAssetImportData(UAssetImportData* InAssetImportData)
{
	AssetImportData = InAssetImportData;
}

#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR

void UVoxel::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	static const FName NAME_Mesh = GET_MEMBER_NAME_CHECKED(UVoxel, Mesh);
	static const FName NAME_Meshes = GET_MEMBER_NAME_CHECKED(UVoxel, Meshes);

	const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (PropertyName == NAME_Mesh || PropertyName == NAME_Meshes)
	{
		CalcCellBounds();
	}

	// Every edit (details panel, Voxel Editor, undo / redo); a dragged value is applied when it is released.
	if ((PropertyChangedEvent.ChangeType & EPropertyChangeType::Interactive) == 0)
	{
		RefreshComponents();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UVoxel::RefreshComponents()
{
	++ChangeCount;
	for (TObjectIterator<UVoxelComponent> It; It; ++It)
	{
		if (It->GetVoxel() == this)
		{
			It->SetVoxel(this, true); // templates only keep the asset; instances rebuild their cubes
		}
	}
}

void UVoxel::CalcCellBounds()
{
	FBoxSphereBounds Bounds(ForceInit);
	bool bFirst = true;
	for (int32 Colour = INDEX_NONE; Colour < Meshes.Num(); ++Colour)
	{
		const UStaticMesh* Cube = Colour == INDEX_NONE ? Mesh.Get() : Meshes[Colour].Get();
		if (!Cube)
		{
			continue;
		}
		// Adding to a ForceInit (zero) bounds would always include the origin.
		Bounds = bFirst ? Cube->GetBounds() : Bounds + Cube->GetBounds();
		bFirst = false;
	}
	CellBounds = Bounds;
}

#endif // WITH_EDITOR
