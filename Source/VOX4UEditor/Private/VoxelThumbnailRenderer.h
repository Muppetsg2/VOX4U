// Copyright 2016-2018 mik14a / Admix Network. All Rights Reserved.
// Edited by Muppetsg2 2025-2026

#pragma once

#include "CoreMinimal.h"
#include "ThumbnailHelpers.h"
#include "ThumbnailRendering/DefaultSizedThumbnailRenderer.h"
#include "VoxelThumbnailRenderer.generated.h"

class AVoxelActor;
class UVoxel;

class FVoxelThumbnailScene : public FThumbnailPreviewScene
{
public:

	FVoxelThumbnailScene();

	void SetVoxel(UVoxel* Voxel);

protected:

	virtual void GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const override;

private:

	AVoxelActor* Actor;

};

/**
 * Voxel asset thumbnail renderer
 */
UCLASS()
class UVoxelThumbnailRenderer : public UDefaultSizedThumbnailRenderer
{
	GENERATED_BODY()

public:

	virtual void Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily) override;

	virtual void BeginDestroy() override;

private:

	TMap<TWeakObjectPtr<UVoxel>, FVoxelThumbnailScene*> ThumbnailScenes;

	/** UVoxel::GetChangeCount() of the asset when its thumbnail scene was built. */
	TMap<TWeakObjectPtr<UVoxel>, uint32> ThumbnailChangeCounts;

};
