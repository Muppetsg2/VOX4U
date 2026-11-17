// Copyright 2016-2018 mik14a / Admix Network. All Rights Reserved.
// Edited by Muppetsg2 2025-2026

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "Voxel.generated.h"

class UAssetImportData;
class UMaterialInterface;
class UStaticMesh;

/**
 * VOXEL Asset: a grid of voxels drawn with one cube mesh; every voxel has one of the colours, and every colour is a
 * material. Imported from .vox files or created in the Content Browser, and edited in the Voxel Editor.
 *
 * Cell C covers [C * GetCellSize() - GetGridOffset(), (C + 1) * GetCellSize() - GetGridOffset()] in the space of
 * the voxel component.
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

	/** Size of one voxel in Unreal units (cm), as the Scale import option; the cube meshes are scaled to it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel", Meta = (DisplayName = "Scale", ClampMin = "0.001", UIMin = "0.001", ToolTip = "Size of one voxel in Unreal units (cm), as the Scale import option. The cube meshes are scaled to it."))
	float Scale;

	UPROPERTY(EditDefaultsOnly, Category = "Voxel", Meta = (DisplayName = "XY Center", EditCondition = "!bUsePivot", ToolTip = "Center the voxels horizontally around the actor (assets without a pivot)."))
	uint32 bXYCenter : 1;

	/** Place the voxels around Pivot instead of using XY Center. */
	UPROPERTY(EditDefaultsOnly, Category = "Voxel", Meta = (DisplayName = "Use Pivot", ToolTip = "Place the voxels around Pivot instead of using XY Center."))
	uint32 bUsePivot : 1;

	/** Point of the voxel grid (in voxels, from the grid corner) that lies at the origin of the component. */
	UPROPERTY(EditDefaultsOnly, Category = "Voxel", Meta = (DisplayName = "Pivot", EditCondition = "bUsePivot", ToolTip = "Point of the voxel grid, in voxels from its corner, that lies at the origin of the component."))
	FVector Pivot;

	/** Cube drawn for every voxel (scaled to Scale); the colours come from Materials. */
	UPROPERTY(EditDefaultsOnly, Category = "Voxel", Meta = (DisplayName = "Mesh", ToolTip = "Cube mesh drawn for every voxel, scaled to Scale. The colours come from Materials."))
	TObjectPtr<UStaticMesh> Mesh;

	/** The colours: one material per colour; Voxels store indices into this array. */
	UPROPERTY(EditDefaultsOnly, EditFixedSize, Category = "Voxel", Meta = (DisplayName = "Materials", ToolTip = "One material per colour; the voxels store indices into this list. None: the material of the mesh."))
	TArray<TObjectPtr<UMaterialInterface>> Materials;

	/**
	 * Assets imported before VOX4U 0.6.0: one cube per colour, with the colour in the cube. A colour uses its cube
	 * instead of Mesh; reimporting the asset replaces them with Mesh and Materials.
	 */
	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "Voxel", Meta = (DisplayName = "Legacy Meshes", ToolTip = "Assets imported before VOX4U 0.6.0: one cube per colour, used instead of Mesh. Reimporting the asset replaces them with Mesh and Materials."))
	TArray<TObjectPtr<UStaticMesh>> Meshes;

	UPROPERTY(EditDefaultsOnly, Category = "Voxel", Meta = (DisplayName = "Voxels", ToolTip = "Voxel cells and the index of their mesh."))
	TMap<FIntVector, uint8> Voxels;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Instanced, Category = "Import Settings", Meta = (DisplayName = "Asset Import Data", ToolTip = "Source file and options used to reimport the asset."))
	TObjectPtr<UAssetImportData> AssetImportData;
#endif // WITH_EDITORONLY_DATA

public:

	UVoxel();

	virtual void PostLoad() override;

	/** Size of one voxel: the size of the cube meshes times GetMeshScale(). */
	FVector GetCellSize() const;

	/** Scale of the cube meshes that makes a voxel Scale units large (1 if Scale is not set). */
	double GetMeshScale() const;

	/** Shift of the grid in the component (pivot or XY center), see the class comment. */
	FVector GetGridOffset() const;

	/** Box of a cell in the space of the voxel component. */
	FBox GetCellBox(const FIntVector& Cell) const;

	/** Cell that contains a point given in the space of the voxel component. */
	FIntVector GetCellAt(const FVector& Position) const;

	/** Number of colours (Materials). */
	int32 GetNumColours() const;

	/** Cube drawn for a colour: Mesh, or the colour's own cube for assets imported before 0.6.0. */
	UStaticMesh* GetColourMesh(int32 ColourIndex) const;

	/** Material of a colour; nullptr for the material of its mesh. */
	UMaterialInterface* GetMaterial(int32 ColourIndex) const;

#if WITH_EDITORONLY_DATA

	UAssetImportData* GetAssetImportData() const;

	void SetAssetImportData(UAssetImportData* InAssetImportData);

#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

	void CalcCellBounds();

	/** Rebuilds every voxel component that shows this asset (levels, Blueprints, previews) after a change. */
	void RefreshComponents();

	/** Grows with every change of the asset (RefreshComponents); thumbnails compare it to rebuild their scene. */
	uint32 GetChangeCount() const { return ChangeCount; }

#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA

private:

	uint32 ChangeCount = 0;

#endif // WITH_EDITORONLY_DATA
};
