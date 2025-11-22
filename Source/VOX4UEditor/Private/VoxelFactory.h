// Copyright 2016-2018 mik14a / Admix Network. All Rights Reserved.
// Edited by Muppetsg2 2025

#pragma once

#include <CoreMinimal.h>
#include <EditorReimportHandler.h>
#include <Factories/Factory.h>
#include <RawMesh.h>
#include "VoxelFactory.generated.h"

struct FVox;
class UMaterialInterface;
class USkeletalMesh;
class UStaticMesh;
class UVoxImportOption;
class UVoxel;

/**
 * Factory
 */
UCLASS()
class UVoxelFactory : public UFactory, public FReimportHandler
{
	GENERATED_BODY()

public:

	UVoxelFactory(const FObjectInitializer& ObjectInitializer);

	void PostInitProperties() override;

	virtual bool DoesSupportClass(UClass* Class) override;

	virtual UClass* ResolveSupportedClass() override;

	virtual UObject* FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn) override;

	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;

	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;

	virtual EReimportResult::Type Reimport(UObject* Obj) override;

private:

	UStaticMesh* CreateStaticMesh(UObject* InParent, FName InName, EObjectFlags Flags, const FVox* Vox, const uint32 ModelId) const;

	TArray<UStaticMesh*> CreateStaticMeshes(UObject* InParent, FName InName, EObjectFlags Flags, const FVox* Vox) const;

	UVoxel* CreateVoxel(UObject* InParent, FName InName, EObjectFlags Flags, const FVox* Vox, const uint32 ModelId) const;

	TArray<UVoxel*> CreateVoxels(UObject* InParent, FName InName, EObjectFlags Flags, const FVox* Vox) const;

	UStaticMesh* BuildStaticMesh(UStaticMesh* OutStaticMesh, FRawMesh& RawMesh) const;

	UMaterialInterface* CreateMaterial(UObject* InParent, FName& InName, EObjectFlags Flags, const FVox* Vox) const;

	void GenerateMaterials(UObject* InParent, FName& InName, EObjectFlags Flags, const FVox* Vox, TArray<uint8>& OutPalette) const;

protected:

	UPROPERTY()
	UVoxImportOption* ImportOption;
	bool bShowOption;
};