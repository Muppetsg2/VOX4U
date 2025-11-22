// Copyright 2016-2018 mik14a / Admix Network. All Rights Reserved.
// Edited by Muppetsg2 2025

#pragma once

#include <CoreMinimal.h>
#include <UObject/NoExportTypes.h>
#include <Delegates/DelegateSignatureImpl.inl>
#include "Voxel.generated.h"

class UStaticMesh;

/**
 * VOXEL Asset
 */
UCLASS()
class VOX4U_API UVoxel : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Voxel)
	FIntVector Size;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Voxel)
	FBoxSphereBounds CellBounds;

	UPROPERTY(EditDefaultsOnly, Category = Voxel)
	uint32 bXYCenter : 1;

	UPROPERTY(EditDefaultsOnly, EditFixedSize, Category = Voxel)
	TArray<UStaticMesh*> Meshes;

	UPROPERTY(EditDefaultsOnly, Category = Voxel)
	TMap<FIntVector, uint8> Voxels;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Instanced, Category = ImportSettings)
	TObjectPtr<class UAssetImportData> AssetImportData;
#endif // WITH_EDITORONLY_DATA

public:

	UVoxel();

#if WITH_EDITORONLY_DATA

	class UAssetImportData* GetAssetImportData() const;

	void SetAssetImportData(class UAssetImportData* InAssetImportData);

#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

	void CalcCellBounds();

#endif // WITH_EDITOR
};