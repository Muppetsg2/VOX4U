// Copyright 2016-2018 mik14a / Admix Network. All Rights Reserved.
// Edited by Muppetsg2 2025-2026

#include "VoxelThumbnailRenderer.h"
#include "EngineModule.h"
#include "SceneInterface.h"
#include "SceneView.h"
#include "ThumbnailRendering/SceneThumbnailInfo.h"
#include "VOX4UVersion.h"
#include "Voxel.h"
#include "VoxelActor.h"
#include "VoxelComponent.h"
#if !VOX4U_UE_AT_LEAST(5, 1)
#include "LegacyScreenPercentageDriver.h"
#include "RendererInterface.h"
#endif

FVoxelThumbnailScene::FVoxelThumbnailScene()
	: Actor(nullptr)
{
	bForceAllUsedMipsResident = false;
	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnInfo.bNoFail = true;
	SpawnInfo.ObjectFlags = RF_Transient;
	Actor = GetWorld()->SpawnActor<AVoxelActor>(SpawnInfo);
	Actor->GetVoxelComponent()->SetMobility(EComponentMobility::Movable);
	Actor->SetActorEnableCollision(false);
}

void FVoxelThumbnailScene::SetVoxel(UVoxel* Voxel)
{
	Actor->GetVoxelComponent()->SetVoxel(Voxel, true);
	if (Voxel)
	{
		Actor->SetActorLocation(FVector(0, 0, 0), false);
		Actor->GetVoxelComponent()->UpdateBounds();
		const float BoundsZOffset = GetBoundsZOffset(Actor->GetVoxelComponent()->Bounds);
		Actor->SetActorLocation(-Actor->GetVoxelComponent()->Bounds.Origin + FVector(0, 0, BoundsZOffset), false);
		Actor->GetVoxelComponent()->RecreateRenderState_Concurrent();
		Actor->PostEditChange();
	}
}

void FVoxelThumbnailScene::GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const
{
	check(Actor);
	check(Actor->GetVoxelComponent());

	const float HalfFOVRadians = FMath::DegreesToRadians<float>(InFOVDegrees) * 0.5f;
	const float HalfMeshSize = Actor->GetVoxelComponent()->Bounds.SphereRadius * 1.15f;
	const float BoundsZOffset = GetBoundsZOffset(Actor->GetVoxelComponent()->Bounds);
	const float TargetDistance = HalfMeshSize / FMath::Tan(HalfFOVRadians);

	USceneThumbnailInfo* ThumbnailInfo = USceneThumbnailInfo::StaticClass()->GetDefaultObject<USceneThumbnailInfo>();
	OutOrigin = FVector(0, 0, -BoundsZOffset);
	OutOrbitPitch = ThumbnailInfo->OrbitPitch;
	OutOrbitYaw = ThumbnailInfo->OrbitYaw;
	OutOrbitZoom = TargetDistance + ThumbnailInfo->OrbitZoom;
}

void UVoxelThumbnailRenderer::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget* RenderTarget, FCanvas* Canvas, bool bAdditionalViewFamily)
{
	UVoxel* Voxel = Cast<UVoxel>(Object);
	if (!Voxel || !IsValid(Voxel))
	{
		return;
	}

	FVoxelThumbnailScene* ThumbnailScene = ThumbnailScenes.FindRef(Voxel);
	if (!ThumbnailScene)
	{
		ThumbnailScene = new FVoxelThumbnailScene();
		ThumbnailScene->SetVoxel(Voxel);
		ThumbnailScenes.Add(Voxel, ThumbnailScene);
	}

	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(RenderTarget, ThumbnailScene->GetScene(), FEngineShowFlags(ESFIM_Game))
		.SetTime(UThumbnailRenderer::GetTime())
		.SetAdditionalViewFamily(bAdditionalViewFamily));
	ViewFamily.EngineShowFlags.DisableAdvancedFeatures();
	ViewFamily.EngineShowFlags.MotionBlur = 0;
	ViewFamily.EngineShowFlags.LOD = 0;

#if VOX4U_UE_AT_LEAST(5, 1)
	RenderViewFamily(Canvas, &ViewFamily, ThumbnailScene->CreateView(&ViewFamily, X, Y, Width, Height));
#else
	// UE 5.0: UThumbnailRenderer::RenderViewFamily(Canvas, Family, View) does not exist yet.
	ViewFamily.EngineShowFlags.ScreenPercentage = false;
	ViewFamily.SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(ViewFamily, 1.0f, false));
	ThumbnailScene->CreateView(&ViewFamily, X, Y, Width, Height);
	GetRendererModule().BeginRenderingViewFamily(Canvas, &ViewFamily);
#endif
}

void UVoxelThumbnailRenderer::BeginDestroy()
{
	for (TPair<TWeakObjectPtr<UVoxel>, FVoxelThumbnailScene*>& Pair : ThumbnailScenes)
	{
		delete Pair.Value;
	}
	ThumbnailScenes.Empty();

	Super::BeginDestroy();
}
