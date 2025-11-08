// Copyright 2016-2018 mik14a / Admix Network. All Rights Reserved.
// Edited by Muppetsg2 2025

#include "Voxel.h"
#include <Engine/StaticMesh.h>

UVoxel::UVoxel()
	: Size(ForceInit)
	, CellBounds(FVector::ZeroVector, FVector(100.f, 100.f, 100.f), 100.f)
	, bXYCenter(true)
	, Mesh()
	, Voxel()
{
}

#if WITH_EDITORONLY_DATA

	/**
	 * Get AssetImportData for the voxel mesh
	 */
	class UAssetImportData* UVoxel::GetAssetImportData() const
	{
		return AssetImportData;
	}

	/**
	 * Set AssetImportData for the voxel mesh
	 */
	void UVoxel::SetAssetImportData(class UAssetImportData* InAssetImportData)
	{
		AssetImportData = InAssetImportData;
	}

#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR

void UVoxel::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	static const FName NAME_Mesh = GET_MEMBER_NAME_CHECKED(UVoxel, Mesh);

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == NAME_Mesh) {
		CalcCellBounds();
	}
}

void UVoxel::CalcCellBounds()
{
	FBoxSphereBounds Bounds(ForceInit);
	for (const auto* Cell : Mesh.FilterByPredicate([](UStaticMesh* m) { return !!m; })) {
		Bounds = Bounds + Cell->GetBounds();
	}
	CellBounds = Bounds;
}

#endif // WITH_EDITOR