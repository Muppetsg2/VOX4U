// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "VoxImportOption.h"

/** Where the assets of one import go. */
struct FVoxAssetLocation
{
	/** Folder of the main asset(s), e.g. /Game/Voxels */
	FString PackagePath;
	/** Folder of materials, textures and voxel cubes (PackagePath or PackagePath/Name_Resources). */
	FString ResourcesPath;
	/** Base name all asset names are generated from. */
	FString BaseName;
	EVoxNamingConvention Naming = EVoxNamingConvention::UnrealEngine;
	EObjectFlags Flags = RF_NoFlags;

	static FVoxAssetLocation Make(const FString& PackagePath, const FString& BaseName, const FVoxImportSettings& Settings, EObjectFlags Flags)
	{
		FVoxAssetLocation Location;
		Location.PackagePath = PackagePath;
		Location.BaseName = BaseName;
		Location.Naming = Settings.NamingConvention;
		Location.Flags = Flags;
		Location.ResourcesPath = Settings.ResourcesSaveLocation == EVoxResourcesSaveLocation::SubFolder
			? PackagePath / FString::Printf(TEXT("%s_Resources"), *BaseName)
			: PackagePath;
		return Location;
	}
};

namespace VoxAssets
{
	/**
	 * Returns the asset Folder/Name of class T, loading it if it exists, creating it otherwise.
	 * Existing assets are reused (and updated by the caller) so that reimports keep references intact.
	 * Returns nullptr if an asset of another class has that name.
	 */
	template <class T>
	T* FindOrCreate(const FString& Folder, const FString& Name, EObjectFlags Flags, bool& bOutCreated)
	{
		bOutCreated = false;
		const FString PackageName = Folder / Name;
		const FString ObjectPath = PackageName + TEXT(".") + Name;

		if (UObject* Existing = StaticLoadObject(UObject::StaticClass(), nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			if (T* Typed = Cast<T>(Existing))
			{
				Typed->Modify();
				return Typed;
			}
			UE_LOG(LogTemp, Error, TEXT("VOX4U: %s already exists and is a %s, not a %s."),
				*ObjectPath, *Existing->GetClass()->GetName(), *T::StaticClass()->GetName());
			return nullptr;
		}

		UPackage* Package = CreatePackage(*PackageName);

		T* Object = NewObject<T>(Package, FName(*Name), Flags | RF_Public | RF_Standalone | RF_Transactional);
		bOutCreated = true;
		return Object;
	}

	/** Registers a new asset and marks its package dirty. */
	inline void Finish(UObject* Object, bool bCreated)
	{
		if (!Object)
		{
			return;
		}
		if (bCreated)
		{
			FAssetRegistryModule::AssetCreated(Object);
		}
		Object->MarkPackageDirty();
	}
}
