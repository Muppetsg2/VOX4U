// Copyright 2016-2018 mik14a / Admix Network. All Rights Reserved.
// Edited by Muppetsg2 2025-2026

#include "Voxel.h"
#include "Engine/StaticMesh.h"
#if WITH_EDITORONLY_DATA
#include "EditorFramework/AssetImportData.h"
#endif

UVoxel::UVoxel()
	: Size(ForceInit)
	, CellBounds(FVector::ZeroVector, FVector(100.f, 100.f, 100.f), 100.f)
	, bXYCenter(true)
	, bUsePivot(false)
	, Pivot(FVector::ZeroVector)
	, Meshes()
	, Voxels()
{
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
	Super::PostEditChangeProperty(PropertyChangedEvent);

	static const FName NAME_Meshes = GET_MEMBER_NAME_CHECKED(UVoxel, Meshes);

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == NAME_Meshes)
	{
		CalcCellBounds();
	}
}

void UVoxel::CalcCellBounds()
{
	FBoxSphereBounds Bounds(ForceInit);
	bool bFirst = true;
	for (UStaticMesh* Mesh : Meshes)
	{
		if (!Mesh)
		{
			continue;
		}
		// Adding to a ForceInit (zero) bounds would always include the origin.
		Bounds = bFirst ? Mesh->GetBounds() : Bounds + Mesh->GetBounds();
		bFirst = false;
	}
	CellBounds = Bounds;
}

#endif // WITH_EDITOR
