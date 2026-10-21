// Copyright 2016-2018 mik14a / Admix Network. All Rights Reserved.
// Edited by Muppetsg2 2025-2026

#pragma once

#include "CoreMinimal.h"
#include "EditorReimportHandler.h"
#include "Factories/Factory.h"
#include "VoxelFactory.generated.h"

class UVoxAssetImportData;
class UVoxImportOption;

/**
 * Imports MagicaVoxel .vox files (every file version) as static meshes, voxel assets or a scene Blueprint.
 */
UCLASS()
class UVoxelFactory : public UFactory, public FReimportHandler
{
	GENERATED_BODY()

public:

	UVoxelFactory(const FObjectInitializer& ObjectInitializer);

	virtual void PostInitProperties() override;

	virtual bool DoesSupportClass(UClass* Class) override;

	virtual UClass* ResolveSupportedClass() override;

	virtual UObject* FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn) override;

	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;

	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;

	virtual EReimportResult::Type Reimport(UObject* Obj) override;

	/** Called once after all files of an import: the next import starts with the dialog and fresh names. */
	virtual void CleanUp() override;

private:

	static UVoxAssetImportData* GetVoxImportData(UObject* Obj);

	/** UVoxImportOption of the running AssetImportTask, if a script set one, otherwise ImportOption. */
	UVoxImportOption* GetActiveOptions() const;

	/**
	 * Base name of the file being imported. A custom asset name used for several files of one import gets a
	 * number, so that the files do not overwrite each other: with Import All every file is numbered (Tree_1,
	 * Tree_2, ...), with Import a repeated name is numbered from its second use (Tree, Tree_2, ...).
	 */
	FString MakeBaseName(const UVoxImportOption& Options, FName FileName);

protected:

	UPROPERTY()
	TObjectPtr<UVoxImportOption> ImportOption;

	bool bShowOption;

	/** Import All was chosen in this import. */
	bool bImportAllChosen;

	/** How often each custom asset name was used in this import. */
	TMap<FString, int32> CustomNameUses;
};
