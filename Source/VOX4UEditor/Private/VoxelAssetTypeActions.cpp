// Copyright 2016-2018 mik14a / Admix Network. All Rights Reserved.
// Edited by Muppetsg2 2025-2026

#include "VoxelAssetTypeActions.h"
#include "Voxel.h"
#include "VoxelEditorToolkit.h"

#define LOCTEXT_NAMESPACE "VoxelAssetTypeActions"

FVoxelAssetTypeActions::FVoxelAssetTypeActions() {}

FText FVoxelAssetTypeActions::GetName() const
{
	return LOCTEXT("AssetTypeName", "Voxel");
}

UClass* FVoxelAssetTypeActions::GetSupportedClass() const
{
	return UVoxel::StaticClass();
}

FColor FVoxelAssetTypeActions::GetTypeColor() const
{
	return FColor::Cyan;
}

uint32 FVoxelAssetTypeActions::GetCategories()
{
	return EAssetTypeCategories::Misc;
}

bool FVoxelAssetTypeActions::IsImportedAsset() const
{
	return true;
}

void FVoxelAssetTypeActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	const EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;
	for (UObject* Object : InObjects)
	{
		if (UVoxel* Voxel = Cast<UVoxel>(Object))
		{
			// The asset editor subsystem keeps the editor alive while it is open.
			const TSharedRef<FVoxelEditorToolkit> Editor = MakeShared<FVoxelEditorToolkit>();
			Editor->Init(Mode, EditWithinLevelEditor, Voxel);
		}
	}
}

#undef LOCTEXT_NAMESPACE
