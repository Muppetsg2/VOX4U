// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#include "SVoxelEditorViewport.h"
#include "AdvancedPreviewScene.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Voxel.h"
#include "VoxelActor.h"
#include "VoxelComponent.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "VoxelEditorViewport"

FVoxelEditorViewportClient::FVoxelEditorViewportClient(FAdvancedPreviewScene& InPreviewScene, const TSharedRef<SVoxelEditorViewport>& InViewport)
	: FEditorViewportClient(nullptr, &InPreviewScene, StaticCastSharedRef<SEditorViewport>(InViewport))
{
	DrawHelper.bDrawPivot = false;
	DrawHelper.bDrawWorldBox = false;
	DrawHelper.bDrawKillZ = false;

	SetViewMode(VMI_Lit);
	EngineShowFlags.SetSnap(false);
	EngineShowFlags.SetCompositeEditorPrimitives(true);
	OverrideNearClipPlane(1.0f);
	bUsingOrbitCamera = true;
	SetRealtime(true);

	// From the front (MagicaVoxel -Y, imported towards +X), slightly from the right and from above.
	SetViewLocation(FVector(400.0, 250.0, 200.0));
	SetViewRotation(FRotator(-20.0, -150.0, 0.0));
}

void FVoxelEditorViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);

	// The preview world is not ticked by the level editor.
	if (PreviewScene && PreviewScene->GetWorld())
	{
		PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
	}
}

void SVoxelEditorViewport::Construct(const FArguments& InArgs)
{
	Voxel = InArgs._Voxel;

	// Must exist before SEditorViewport::Construct, which creates the viewport client.
	VoxelPreviewScene = MakeShareable(new FAdvancedPreviewScene(FPreviewScene::ConstructionValues(), 0.0f));

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnInfo.bNoFail = true;
	SpawnInfo.ObjectFlags = RF_Transient;
	AVoxelActor* Actor = VoxelPreviewScene->GetWorld()->SpawnActor<AVoxelActor>(SpawnInfo);
	if (Actor)
	{
		Actor->SetActorEnableCollision(false);
	}
	PreviewActor = Actor;

	SEditorViewport::Construct(SEditorViewport::FArguments());

	Refresh(true);
}

SVoxelEditorViewport::~SVoxelEditorViewport()
{
	if (VoxelViewportClient.IsValid())
	{
		VoxelViewportClient->Viewport = nullptr;
	}
}

void SVoxelEditorViewport::Refresh(bool bFrame)
{
	AVoxelActor* Actor = PreviewActor.Get();
	UVoxelComponent* Component = Actor ? Actor->GetVoxelComponent() : nullptr;
	if (!Component)
	{
		return;
	}

	Component->SetVoxel(Voxel.Get(), true);
	Component->UpdateBounds();
	Component->MarkRenderStateDirty();

	// The floor touches the lowest voxel; the actor stays at the origin so that the pivot can be seen.
	const FBoxSphereBounds& VoxelBounds = Component->Bounds;
	VoxelPreviewScene->SetFloorOffset(static_cast<float>(VoxelBounds.BoxExtent.Z - VoxelBounds.Origin.Z));

	if (bFrame)
	{
		FrameVoxels(true);
	}
	if (VoxelViewportClient.IsValid())
	{
		VoxelViewportClient->Invalidate();
	}
}

void SVoxelEditorViewport::FrameVoxels(bool bInstant)
{
	const AVoxelActor* Actor = PreviewActor.Get();
	const UVoxelComponent* Component = Actor ? Actor->GetVoxelComponent() : nullptr;
	if (Component && VoxelViewportClient.IsValid())
	{
		VoxelViewportClient->FocusViewportOnBox(Component->Bounds.GetBox(), bInstant);
	}
}

TSharedRef<FAdvancedPreviewScene> SVoxelEditorViewport::GetPreviewScene() const
{
	return VoxelPreviewScene.ToSharedRef();
}

TSharedRef<SEditorViewport> SVoxelEditorViewport::GetViewportWidget()
{
	return SharedThis(this);
}

TSharedPtr<FExtender> SVoxelEditorViewport::GetExtenders() const
{
	return MakeShareable(new FExtender);
}

void SVoxelEditorViewport::OnFloatingButtonClicked()
{
}

TSharedRef<FEditorViewportClient> SVoxelEditorViewport::MakeEditorViewportClient()
{
	VoxelViewportClient = MakeShareable(new FVoxelEditorViewportClient(*VoxelPreviewScene, SharedThis(this)));
	return VoxelViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SVoxelEditorViewport::MakeViewportToolbar()
{
	// Newer engines turn this toolbar into their new viewport toolbar by themselves.
	return SNew(SCommonEditorViewportToolbarBase, SharedThis(this));
}

void SVoxelEditorViewport::PopulateViewportOverlays(TSharedRef<SOverlay> Overlay)
{
	SEditorViewport::PopulateViewportOverlays(Overlay);

	Overlay->AddSlot()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Left)
		.Padding(FMargin(10.0f, 40.0f, 10.0f, 10.0f))
		[
			SNew(STextBlock)
			.Text(this, &SVoxelEditorViewport::GetStatsText)
			.ShadowOffset(FVector2D(1.0f, 1.0f))
		];
}

void SVoxelEditorViewport::OnFocusViewportToSelection()
{
	FrameVoxels(false);
}

FText SVoxelEditorViewport::GetStatsText() const
{
	const UVoxel* VoxelAsset = Voxel.Get();
	if (!VoxelAsset)
	{
		return FText::GetEmpty();
	}

	int32 DrawnCubes = 0;
	const AVoxelActor* Actor = PreviewActor.Get();
	if (const UVoxelComponent* Component = Actor ? Actor->GetVoxelComponent() : nullptr)
	{
		for (const UInstancedStaticMeshComponent* Proxy : Component->GetInstancedStaticMeshComponent())
		{
			DrawnCubes += Proxy ? Proxy->GetInstanceCount() : 0;
		}
	}

	return FText::Format(LOCTEXT("Stats", "Size: {0} x {1} x {2}\nVoxels: {3}\nColours: {4}\nCubes drawn: {5}"),
		FText::AsNumber(VoxelAsset->Size.X),
		FText::AsNumber(VoxelAsset->Size.Y),
		FText::AsNumber(VoxelAsset->Size.Z),
		FText::AsNumber(VoxelAsset->Voxels.Num()),
		FText::AsNumber(VoxelAsset->Meshes.Num()),
		FText::AsNumber(DrawnCubes));
}

#undef LOCTEXT_NAMESPACE
