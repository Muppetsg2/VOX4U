// Copyright 2016-2018 mik14a / Admix Network. All Rights Reserved.
// Edited by Muppetsg2 2025-2026

#include "VOX4UEditor.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Voxel.h"
#include "VoxelAssetTypeActions.h"
#include "VoxelThumbnailRenderer.h"
#include "VoxLocalization.h"

#define LOCTEXT_NAMESPACE "FVOX4UEditorModule"

namespace VOX4UEditorPrivate
{
	FVoxLocalization& GetLocalization()
	{
		static FVoxLocalization Localization;
		return Localization;
	}
}

void FVOX4UEditorModule::StartupModule()
{
	// Localization/<culture>.po of the editor language
	VOX4UEditorPrivate::GetLocalization().Startup();

	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	VoxelAssetTypeActions = MakeShareable(new FVoxelAssetTypeActions());
	AssetTools.RegisterAssetTypeActions(VoxelAssetTypeActions.ToSharedRef());
	UThumbnailManager::Get().RegisterCustomRenderer(UVoxel::StaticClass(), UVoxelThumbnailRenderer::StaticClass());
}

void FVOX4UEditorModule::ShutdownModule()
{
	VOX4UEditorPrivate::GetLocalization().Shutdown();

	if (FModuleManager::Get().IsModuleLoaded("AssetTools") && VoxelAssetTypeActions.IsValid())
	{
		IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
		AssetTools.UnregisterAssetTypeActions(VoxelAssetTypeActions.ToSharedRef());
	}
	VoxelAssetTypeActions.Reset();

	if (UObjectInitialized())
	{
		UThumbnailManager::Get().UnregisterCustomRenderer(UVoxel::StaticClass());
	}
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FVOX4UEditorModule, VOX4UEditor)
