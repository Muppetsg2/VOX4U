// Copyright 2016-2018 mik14a / Admix Network. All Rights Reserved.
// Edited by Muppetsg2 2025

#pragma once

#include "CoreMinimal.h"
#include <Engine/EngineTypes.h>
#include <UObject/NoExportTypes.h>
#include "VoxImportOption.generated.h"

/** Import mesh type */
UENUM()
enum class EVoxImportType
{
	StaticMesh UMETA(DisplayName = "Static Mesh"),
	// SkeletalMesh UMETA(DisplayName = "Skeletal Mesh"), // Currently not supported
	Voxel UMETA(DisplayName = "Voxel"),
};

/**
 * Import option
 */
UCLASS(config = EditorPerProjectUserSettings, HideCategories = Object)
class UVoxImportOption : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Category = ImportType)
	EVoxImportType VoxImportType;

	UPROPERTY(EditAnywhere, Category = Generic)
	uint32 bImportXForward : 1;

	UPROPERTY(EditAnywhere, Category = Generic)
	uint32 bImportXYCenter : 1;

	UPROPERTY(EditAnywhere, Category = Generic)
	uint32 bImportMaterial : 1;

	UPROPERTY(EditAnywhere, Category = Generic, Meta = (EditCondition = "bImportMaterial && VoxImportType == EVoxImportType::StaticMesh", EditConditionHides))
	uint32 bOneMaterial : 1;

	UPROPERTY()
	float Scale;

public:

	UVoxImportOption();

	bool GetImportOption(bool& bOutImportAll);

	const FMeshBuildSettings& GetBuildSettings() const {
		return BuildSettings;
	}

private:

	FMeshBuildSettings BuildSettings;

	friend class UVoxAssetImportData;

};