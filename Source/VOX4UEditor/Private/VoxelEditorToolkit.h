// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/AssetEditorToolkit.h"

class IDetailsView;
class SDockTab;
class SVoxelEditTools;
class SVoxelEditorViewport;
class UVoxel;
struct FVoxelEditState;

/**
 * Editor of a voxel asset, opened by double clicking it: a 3D viewport, the voxel tools (add, remove and paint
 * voxels with the mouse, edit the colours), the asset's details and the preview scene settings, laid out like the
 * Static Mesh Editor. The preview follows edits, undo and reimports.
 */
class FVoxelEditorToolkit : public FAssetEditorToolkit
{
public:

	virtual ~FVoxelEditorToolkit() override;

	void Init(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UVoxel* InVoxel);

	//~ IToolkit
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

private:

	TSharedRef<SDockTab> SpawnViewportTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnToolsTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnDetailsTab(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnPreviewSettingsTab(const FSpawnTabArgs& Args);

	void OnObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event);
	void OnAssetReimport(UObject* Object);

	TWeakObjectPtr<UVoxel> EditedVoxel;
	TSharedPtr<FVoxelEditState> EditState;
	TSharedPtr<SVoxelEditorViewport> VoxelViewport;
	TSharedPtr<SVoxelEditTools> VoxelTools;
	TSharedPtr<IDetailsView> VoxelDetails;
	TSharedPtr<SWidget> PreviewSettings;

	FDelegateHandle PropertyChangedHandle;
	FDelegateHandle ReimportHandle;
};
