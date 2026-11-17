// Copyright 2016-2018 mik14a / Admix Network. All Rights Reserved.
// Edited by Muppetsg2 2025-2026

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "VoxelComponent.generated.h"

class UInstancedStaticMeshComponent;
class UStaticMesh;
class UVoxel;

/**
 * Voxel component
 */
UCLASS(ClassGroup = Rendering, Meta = (BlueprintSpawnableComponent))
class VOX4U_API UVoxelComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel Component", Meta = (DisplayName = "Cell Bounds", ToolTip = "Bounds of one voxel cube."))
	FBoxSphereBounds CellBounds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel Component", Meta = (DisplayName = "Hide Unbeheld", ToolTip = "Skip voxels surrounded by other voxels on all six sides."))
	bool bHideUnbeheld;

	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadWrite, Category = "Voxel Component", Meta = (DisplayName = "Meshes", ToolTip = "Cube mesh of every colour of the voxel asset."))
	TArray<TObjectPtr<UStaticMesh>> Meshes;

	UPROPERTY(EditAnywhere, EditFixedSize, BlueprintReadWrite, Category = "Voxel Component", Meta = (DisplayName = "Cells", ToolTip = "Voxel cells and the index of their mesh."))
	TMap<FIntVector, uint8> Cells;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Component", Meta = (DisplayName = "Voxel", ToolTip = "Voxel asset to display."))
	TObjectPtr<UVoxel> Voxel;

public:

	UVoxelComponent();

#if WITH_EDITOR

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

#endif // WITH_EDITOR

	void SetVoxel(class UVoxel* InVoxel, bool bForce = false);

	const UVoxel* GetVoxel() const;

	UFUNCTION(BlueprintCallable, Category = Voxel)
	void AddVoxel();

	UFUNCTION(BlueprintCallable, Category = Voxel)
	void ClearVoxel();

	UFUNCTION(BlueprintCallable, Category = Voxel)
	bool IsUnbeheldVolume(const FIntVector& InVector) const;

	UFUNCTION(BlueprintCallable, Category = Voxel)
	bool GetVoxelTransform(const FIntVector& InVector, FTransform& OutVoxelTransform, bool bWorldSpace = false) const;

	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

	virtual void OnRegister() override;

	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

	const TArray<TObjectPtr<UInstancedStaticMeshComponent>>& GetInstancedStaticMeshComponent() const;

private:

	void InitVoxel();

	void DestroyInstancedComponents();

	FVector GetCellTranslation(const FIntVector& InVector) const;

	/** Scale of the cube instances (UVoxel::Scale). */
	double GetMeshScale() const;

protected:

	/** One instanced static mesh per cube mesh, attached to this component; rebuilt when it is registered. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> InstancedStaticMeshComponents;

};
