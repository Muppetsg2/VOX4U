// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "EditorViewportClient.h"
#include "SCommonEditorViewportToolbarBase.h"
#include "SEditorViewport.h"

class AVoxelActor;
class FAdvancedPreviewScene;
class SVoxelEditorViewport;
class UVoxel;

/** Camera and drawing of the voxel editor's viewport (orbit camera, like the Static Mesh Editor). */
class FVoxelEditorViewportClient : public FEditorViewportClient
{
public:

	FVoxelEditorViewportClient(FAdvancedPreviewScene& InPreviewScene, const TSharedRef<SVoxelEditorViewport>& InViewport);

	virtual void Tick(float DeltaSeconds) override;
};

/**
 * 3D preview of a voxel asset: the asset drawn by a Voxel Actor in a preview scene with floor, sky and lights.
 * The usual viewport toolbar (view modes, show flags, camera) and the Preview Scene Settings of the Static Mesh
 * Editor apply; F frames the voxels.
 */
class SVoxelEditorViewport : public SEditorViewport, public ICommonEditorViewportToolbarInfoProvider
{
public:

	SLATE_BEGIN_ARGS(SVoxelEditorViewport)
		: _Voxel(nullptr)
		{}
		SLATE_ARGUMENT(UVoxel*, Voxel)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SVoxelEditorViewport() override;

	/** Rebuilds the preview from the asset (after an edit or a reimport); optionally frames it again. */
	void Refresh(bool bFrame);

	TSharedRef<FAdvancedPreviewScene> GetPreviewScene() const;

	//~ ICommonEditorViewportToolbarInfoProvider
	virtual TSharedRef<SEditorViewport> GetViewportWidget() override;
	virtual TSharedPtr<FExtender> GetExtenders() const override;
	virtual void OnFloatingButtonClicked() override;

protected:

	//~ SEditorViewport
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;
	virtual void PopulateViewportOverlays(TSharedRef<SOverlay> Overlay) override;
	virtual void OnFocusViewportToSelection() override;

private:

	/** Size, voxel count and colours of the asset, cubes drawn by the preview. */
	FText GetStatsText() const;

	void FrameVoxels(bool bInstant);

	TWeakObjectPtr<UVoxel> Voxel;
	TWeakObjectPtr<AVoxelActor> PreviewActor;
	TSharedPtr<FAdvancedPreviewScene> VoxelPreviewScene;
	TSharedPtr<FVoxelEditorViewportClient> VoxelViewportClient;
};
