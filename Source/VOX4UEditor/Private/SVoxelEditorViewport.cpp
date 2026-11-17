// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#include "SVoxelEditorViewport.h"
#include "AdvancedPreviewScene.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "InputCoreTypes.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "Voxel.h"
#include "VoxelActor.h"
#include "VoxelComponent.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "VoxelEditorViewport"

namespace VoxelEditorViewportPrivate
{
	const FLinearColor AddColor(0.1f, 1.0f, 0.2f);
	const FLinearColor RemoveColor(1.0f, 0.15f, 0.1f);
	const FLinearColor PaintColor(1.0f, 0.85f, 0.1f);

	/** Outline of the edited cell, in pixels: the same on small and large voxels. */
	constexpr float OutlineThickness = 2.0f;
	/** Opacity of the visible edges and of the edges hidden behind voxels. */
	constexpr float VisibleOpacity = 0.9f;
	constexpr float HiddenOpacity = 0.25f;
}

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

void FVoxelEditorViewportClient::SetEditTarget(UVoxel* InVoxel, UVoxelComponent* InComponent, const TSharedPtr<FVoxelEditState>& InEditState)
{
	EditedVoxel = InVoxel;
	EditedComponent = InComponent;
	EditState = InEditState;
}

EVoxelEditTool FVoxelEditorViewportClient::GetActiveTool() const
{
	const EVoxelEditTool Tool = EditState.IsValid() ? EditState->Tool : EVoxelEditTool::Select;
	return Tool == EVoxelEditTool::Add && IsShiftPressed() ? EVoxelEditTool::Remove : Tool;
}

bool FVoxelEditorViewportClient::TraceCursor(const FSceneView& View, int32 X, int32 Y, FVoxelRayHit& OutHit)
{
	const UVoxel* Asset = EditedVoxel.Get();
	const UVoxelComponent* Component = EditedComponent.Get();
	if (!Asset || !Component)
	{
		return false;
	}
	const FViewportCursorLocation Cursor(&View, this, X, Y);
	const FTransform& ToWorld = Component->GetComponentTransform();
	return VoxelEditing::Raycast(*Asset, ToWorld.InverseTransformPosition(Cursor.GetOrigin()), ToWorld.InverseTransformVector(Cursor.GetDirection()), OutHit);
}

void FVoxelEditorViewportClient::UpdateHover(int32 X, int32 Y)
{
	const bool bHadHover = bHasHover;
	bHasHover = false;
	if (Viewport && GetActiveTool() != EVoxelEditTool::Select)
	{
		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(Viewport, GetScene(), EngineShowFlags).SetRealtimeUpdate(IsRealtime()));
		if (FSceneView* SceneView = CalcSceneView(&ViewFamily))
		{
			bHasHover = TraceCursor(*SceneView, X, Y, Hover);
		}
	}
	if (bHasHover || bHadHover)
	{
		Invalidate();
	}
}

void FVoxelEditorViewportClient::MouseMove(FViewport* InViewport, int32 X, int32 Y)
{
	FEditorViewportClient::MouseMove(InViewport, X, Y);
	UpdateHover(X, Y);
}

void FVoxelEditorViewportClient::MouseLeave(FViewport* InViewport)
{
	FEditorViewportClient::MouseLeave(InViewport);
	bHasHover = false;
	Invalidate();
}

void FVoxelEditorViewportClient::ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY)
{
	UVoxel* Asset = EditedVoxel.Get();
	const EVoxelEditTool Tool = GetActiveTool();
	if (Key != EKeys::LeftMouseButton || Tool == EVoxelEditTool::Select || !Asset || !EditState.IsValid())
	{
		FEditorViewportClient::ProcessClick(View, HitProxy, Key, Event, HitX, HitY);
		return;
	}

	FVoxelRayHit Hit;
	if (!TraceCursor(View, static_cast<int32>(HitX), static_cast<int32>(HitY), Hit))
	{
		return;
	}

	const int32 Colour = EditState->Colour;
	switch (Tool)
	{
		case EVoxelEditTool::Add:
			if (Hit.bCanAdd && !Asset->Voxels.Contains(Hit.AddCell))
			{
				VoxelEditing::ApplyEdit(*Asset, LOCTEXT("AddVoxel", "Add Voxel"), [&Hit, Colour](UVoxel& Edited) { return VoxelEditing::AddVoxel(Edited, Hit.AddCell, Colour); });
			}
			break;
		case EVoxelEditTool::Remove:
			if (Hit.bHitVoxel)
			{
				VoxelEditing::ApplyEdit(*Asset, LOCTEXT("RemoveVoxel", "Remove Voxel"), [&Hit](UVoxel& Edited) { return VoxelEditing::RemoveVoxel(Edited, Hit.HitCell); });
			}
			break;
		case EVoxelEditTool::Paint:
			if (Hit.bHitVoxel && IsShiftPressed())
			{
				// Pick the colour of the voxel.
				EditState->Colour = Asset->Voxels.FindRef(Hit.HitCell);
				EditState->OnChanged.Broadcast();
			}
			else if (Hit.bHitVoxel && Asset->Voxels.FindRef(Hit.HitCell) != Colour)
			{
				VoxelEditing::ApplyEdit(*Asset, LOCTEXT("PaintVoxel", "Paint Voxel"), [&Hit, Colour](UVoxel& Edited) { return VoxelEditing::PaintVoxel(Edited, Hit.HitCell, Colour); });
			}
			break;
		default:
			break;
	}

	// The grid may have moved (cells below zero are shifted): outline what is under the cursor now.
	bHasHover = TraceCursor(View, static_cast<int32>(HitX), static_cast<int32>(HitY), Hover);
	Invalidate();
}

void FVoxelEditorViewportClient::Draw(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	using namespace VoxelEditorViewportPrivate;

	FEditorViewportClient::Draw(View, PDI);

	const UVoxel* Asset = EditedVoxel.Get();
	const UVoxelComponent* Component = EditedComponent.Get();
	const EVoxelEditTool Tool = GetActiveTool();
	if (!bHasHover || !Asset || !Component || Tool == EVoxelEditTool::Select)
	{
		return;
	}

	const bool bAdding = Tool == EVoxelEditTool::Add;
	if (bAdding ? !Hover.bCanAdd : !Hover.bHitVoxel)
	{
		return;
	}
	const FIntVector Cell = bAdding ? Hover.AddCell : Hover.HitCell;
	const FLinearColor& Color = bAdding ? AddColor : (Tool == EVoxelEditTool::Remove ? RemoveColor : PaintColor);
	const FBox Box = Asset->GetCellBox(Cell).ExpandBy(Asset->GetCellSize().GetMin() * 0.02).TransformBy(Component->GetComponentTransform());

	// Thin lines in screen space, so that the outline never covers the voxels. Edges behind voxels stay faint,
	// visible edges are drawn on top of them.
	FLinearColor Hidden = Color;
	Hidden.A = HiddenOpacity;
	FLinearColor Visible = Color;
	Visible.A = VisibleOpacity;
	DrawWireBox(PDI, Box, Hidden, SDPG_Foreground, OutlineThickness, 0.0f, true);
	DrawWireBox(PDI, Box, Visible, SDPG_World, OutlineThickness, 0.0f, true);
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
	EditState = InArgs._EditState;

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

	if (VoxelViewportClient.IsValid())
	{
		VoxelViewportClient->SetEditTarget(InArgs._Voxel, Actor ? Actor->GetVoxelComponent() : nullptr, EditState);
	}

	Refresh(true, true);
}

SVoxelEditorViewport::~SVoxelEditorViewport()
{
	if (VoxelViewportClient.IsValid())
	{
		VoxelViewportClient->Viewport = nullptr;
	}
}

void SVoxelEditorViewport::Refresh(bool bFrame, bool bRebuild)
{
	AVoxelActor* Actor = PreviewActor.Get();
	UVoxelComponent* Component = Actor ? Actor->GetVoxelComponent() : nullptr;
	if (!Component)
	{
		return;
	}

	if (bRebuild || Component->GetVoxel() != Voxel.Get())
	{
		Component->SetVoxel(Voxel.Get(), true);
	}
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
		FText::AsNumber(VoxelAsset->GetNumColours()),
		FText::AsNumber(DrawnCubes));
}

#undef LOCTEXT_NAMESPACE
