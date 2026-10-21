// Copyright 2016-2018 mik14a / Admix Network. All Rights Reserved.
// Edited by Muppetsg2 2025-2026

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"
#include "Toolkits/IToolkitHost.h"

class FVoxelAssetTypeActions : public FAssetTypeActions_Base
{
public:
	FVoxelAssetTypeActions();

	virtual FText GetName() const override;

	virtual UClass* GetSupportedClass() const override;

	virtual FColor GetTypeColor() const override;

	virtual uint32 GetCategories() override;

	virtual bool IsImportedAsset() const override;

	/** Double click: the voxel editor (3D preview) instead of a plain property window. */
	using FAssetTypeActions_Base::OpenAssetEditor;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	
};
