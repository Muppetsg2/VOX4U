// Copyright 2016-2018 mik14a / Admix Network. All Rights Reserved.
// Edited by Muppetsg2 2025

#pragma once

#include <CoreMinimal.h>
#include <Engine/EngineTypes.h>
#include <UObject/NoExportTypes.h>
#include "VoxImportOption.generated.h"

/** Import mesh type */
UENUM()
enum class EVoxImportType : uint8
{
	StaticMesh UMETA(DisplayName = "Static Mesh"),
	Voxel UMETA(DisplayName = "Voxel")
};

/** Assets Naming Convention */
UENUM()
enum class EVoxNamingConvention : uint8
{
	UnrealEngine UMETA(DisplayName = "Unreal Engine"),
	VOX4U UMETA(DisplayName = "VOX4U")
};

/** Resources save location */
UENUM()
enum class EVoxResourcesSaveLocation : uint8
{
	SubFolder UMETA(DisplayName = "In Subfolder (MeshName_Resources)"),
	SameFolder UMETA(DisplayName = "In Same Folder as Mesh")
};

/** Import option */
UCLASS(config = EditorPerProjectUserSettings, HideCategories = Object)
class UVoxImportOption : public UObject
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Category = "ImportType")
	EVoxImportType VoxImportType;

	UPROPERTY(EditAnywhere, Category = "Assets Names")
	EVoxNamingConvention AssetsNamingConvention;

	UPROPERTY(EditAnywhere, Category = "Assets Names", Meta = (DisplayName = "Custom Asset Name", ToolTip = "Optional custom name for the imported asset"))
	FString CustomAssetName;

	UPROPERTY(EditAnywhere, Category = "Generic")
	EVoxResourcesSaveLocation ResourcesSaveLocation;

	UPROPERTY(EditAnywhere, Category = "Generic")
	uint32 bImportXForward : 1;

	UPROPERTY(EditAnywhere, Category = "Generic")
	uint32 bImportXYCenter : 1;

	UPROPERTY(EditAnywhere, Category = "Generic")
	uint32 bSeparateModels : 1;

	UPROPERTY(EditAnywhere, Category = "Generic")
	float Scale;

	UPROPERTY(EditAnywhere, Category = "Materials")
	uint32 bImportMaterial : 1;

	UPROPERTY(EditAnywhere, Category = "Materials", Meta = (EditCondition = "bImportMaterial", EditConditionHides))
	uint32 bOneMaterial : 1;

	UPROPERTY(EditAnywhere, Category = "Materials", Meta = (EditCondition = "bImportMaterial && !bOneMaterial", EditConditionHides))
	uint32 bPaletteToTexture : 1;

public:

	UVoxImportOption();

	bool GetImportOption(bool& bOutImportAll);

	const FMeshBuildSettings& GetBuildSettings() const 
	{
		return BuildSettings;
	}

private:

	FMeshBuildSettings BuildSettings;

	friend class UVoxAssetImportData;
};