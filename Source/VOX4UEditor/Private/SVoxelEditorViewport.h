// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "EditorViewportClient.h"
#include "SCommonEditorViewportToolbarBase.h"
#include "SEditorViewport.h"
#include "VoxelEditing.h"

class AVoxelActor;
class FAdvancedPreviewScene;
class SVoxelEditorViewport;
class UVoxel;
class UVoxelComponent;

/**
 * Camera and drawing of the voxel editor's viewport (orbit camera, like the Static Mesh Editor). With an edit tool
 * a left click edits the voxel under the cursor, which is outlined: green where a voxel would be added, red for
 * the voxel that would be removed, yellow for the voxel that would be painted.
 */
class FVoxelEditorViewportClient : public FEditorViewportClient
{
public:

	FVoxelEditorViewportClient(FAdvancedPreviewScene& InPreviewScene, const TSharedRef<SVoxelEditorViewport>& InViewport);

	/** Asset and component the clicks edit. */
	void SetEditTarget(UVoxel* InVoxel, UVoxelComponent* InComponent, const TSharedPtr<FVoxelEditState>& InEditState);

	virtual void Tick(float DeltaSeconds) override;
	virtual void ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY) override;
	virtual void MouseMove(FViewport* InViewport, int32 X, int32 Y) override;
	virtual void MouseLeave(FViewport* InViewport) override;
	virtual void Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI) override;

private:

	/** Tool of a click: Shift turns Add into Remove. */
	EVoxelEditTool GetActiveTool() const;

	/** Ray cast from a pixel of the viewport against the voxels. */
	bool TraceCursor(const FSceneView& View, int32 X, int32 Y, FVoxelRayHit& OutHit);

	void UpdateHover(int32 X, int32 Y);

	TWeakObjectPtr<UVoxel> EditedVoxel;
	TWeakObjectPtr<UVoxelComponent> EditedComponent;
	TSharedPtr<FVoxelEditState> EditState;

	bool bHasHover = false;
	FVoxelRayHit Hover;
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
		SLATE_ARGUMENT(TSharedPtr<FVoxelEditState>, EditState)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SVoxelEditorViewport() override;

	/**
	 * Updates the preview after the asset changed. Edits rebuild the voxel component themselves
	 * (UVoxel::RefreshComponents); bRebuild rebuilds it here too (reimport). Optionally frames the voxels again.
	 */
	void Refresh(bool bFrame, bool bRebuild);

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
	TSharedPtr<FVoxelEditState> EditState;
	TWeakObjectPtr<AVoxelActor> PreviewActor;
	TSharedPtr<FAdvancedPreviewScene> VoxelPreviewScene;
	TSharedPtr<FVoxelEditorViewportClient> VoxelViewportClient;
};
