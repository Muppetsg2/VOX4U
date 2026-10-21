// Copyright 2016-2018 mik14a / Admix Network. All Rights Reserved.
// Edited by Muppetsg2 2025-2026

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "Voxel.generated.h"

class UAssetImportData;
class UStaticMesh;

/**
 * VOXEL Asset
 */
UCLASS()
class VOX4U_API UVoxel : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel", Meta = (DisplayName = "Size", ToolTip = "Size of the voxel grid."))
	FIntVector Size;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel", Meta = (DisplayName = "Cell Bounds", ToolTip = "Bounds of one voxel cube."))
	FBoxSphereBounds CellBounds;

	UPROPERTY(EditDefaultsOnly, Category = "Voxel", Meta = (DisplayName = "XY Center", EditCondition = "!bUsePivot", ToolTip = "Center the voxels horizontally around the actor (assets without a pivot)."))
	uint32 bXYCenter : 1;

	/** Place the voxels around Pivot instead of using XY Center. */
	UPROPERTY(EditDefaultsOnly, Category = "Voxel", Meta = (DisplayName = "Use Pivot", ToolTip = "Place the voxels around Pivot instead of using XY Center."))
	uint32 bUsePivot : 1;

	/** Point of the voxel grid (in voxels, from the grid corner) that lies at the origin of the component. */
	UPROPERTY(EditDefaultsOnly, Category = "Voxel", Meta = (DisplayName = "Pivot", EditCondition = "bUsePivot", ToolTip = "Point of the voxel grid, in voxels from its corner, that lies at the origin of the component."))
	FVector Pivot;

	// One cube mesh per material; Voxels store indices into this array.
	UPROPERTY(EditDefaultsOnly, EditFixedSize, Category = "Voxel", Meta = (DisplayName = "Meshes", ToolTip = "One cube mesh per colour."))
	TArray<TObjectPtr<UStaticMesh>> Meshes;

	UPROPERTY(EditDefaultsOnly, Category = "Voxel", Meta = (DisplayName = "Voxels", ToolTip = "Voxel cells and the index of their mesh."))
	TMap<FIntVector, uint8> Voxels;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Instanced, Category = "Import Settings", Meta = (DisplayName = "Asset Import Data", ToolTip = "Source file and options used to reimport the asset."))
	TObjectPtr<UAssetImportData> AssetImportData;
#endif // WITH_EDITORONLY_DATA

public:

	UVoxel();

#if WITH_EDITORONLY_DATA

	UAssetImportData* GetAssetImportData() const;

	void SetAssetImportData(UAssetImportData* InAssetImportData);

#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

	void CalcCellBounds();

#endif // WITH_EDITOR
};
