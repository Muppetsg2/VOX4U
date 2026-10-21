// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#include "VoxelAnimationComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Voxel.h"
#include "VoxelAnimation.h"
#include "VoxelComponent.h"
#if WITH_EDITOR
#include "Engine/SimpleConstructionScript.h"
#include "UObject/UObjectIterator.h"
#endif

namespace VoxelAnimationComponentPrivate
{
	/** Components that show a model the animation can switch. */
	bool ShowsModel(const USceneComponent* Component)
	{
		return Cast<UStaticMeshComponent>(Component) || Cast<UVoxelComponent>(Component);
	}
}

UVoxelAnimationComponent::UVoxelAnimationComponent()
	: Animation(nullptr)
	, bAutoPlay(true)
	, bLoop(true)
	, PlayRate(1.0f)
	, Frame(0.0f)
	, bPlaying(false)
	, PreviewStartFrame(0.0f)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	// Ticks in the editor only while a preview plays (the tick is enabled by Play).
	bTickInEditor = true;
}

bool UVoxelAnimationComponent::IsEditorPreview() const
{
	const UWorld* World = GetWorld();
	return !World || !World->IsGameWorld();
}

#if WITH_EDITOR
void UVoxelAnimationComponent::ForEachPreviewInstance(TFunctionRef<void(UVoxelAnimationComponent&)> Action)
{
	// Template of a Blueprint component: its outer is the Blueprint class, its name the variable name + suffix.
	const UClass* TemplateOwnerClass = GetTypedOuter<UClass>();
	FString InstanceName = GetName();
	InstanceName.RemoveFromEnd(USimpleConstructionScript::ComponentTemplateNameSuffix);

	for (TObjectIterator<UVoxelAnimationComponent> It; It; ++It)
	{
		UVoxelAnimationComponent* Instance = *It;
		if (!Instance || Instance == this || Instance->IsTemplate())
		{
			continue;
		}
		const UWorld* InstanceWorld = Instance->GetWorld();
		if (!InstanceWorld || InstanceWorld->WorldType != EWorldType::EditorPreview)
		{
			continue; // only the Blueprint editor's viewport, not the level
		}
		const AActor* InstanceOwner = Instance->GetOwner();
		const bool bMadeFromThis = Instance->IsBasedOnArchetype(this)
			|| (TemplateOwnerClass && InstanceOwner && InstanceOwner->GetClass()->IsChildOf(TemplateOwnerClass) && Instance->GetName() == InstanceName);
		if (bMadeFromThis)
		{
			Action(*Instance);
		}
	}
}
#endif

void UVoxelAnimationComponent::Play()
{
#if WITH_EDITOR
	if (IsTemplate())
	{
		ForEachPreviewInstance([](UVoxelAnimationComponent& Instance) { Instance.Play(); });
		return;
	}
#endif
	if (!Animation || bPlaying)
	{
		return;
	}
	if (IsEditorPreview())
	{
		PreviewStartFrame = Frame;
	}
	bPlaying = true;
	SetComponentTickEnabled(true);
}

void UVoxelAnimationComponent::Stop()
{
#if WITH_EDITOR
	if (IsTemplate())
	{
		ForEachPreviewInstance([](UVoxelAnimationComponent& Instance) { Instance.Stop(); });
		return;
	}
#endif
	if (!bPlaying)
	{
		return;
	}
	bPlaying = false;
	SetComponentTickEnabled(false);

	// A preview in the editor does not change the level: back to the frame it started from.
	if (IsEditorPreview())
	{
		SetFrame(PreviewStartFrame);
	}
}

bool UVoxelAnimationComponent::IsPlaying() const
{
	return bPlaying;
}

void UVoxelAnimationComponent::SetFrame(float NewFrame)
{
	Frame = NewFrame;
	ApplyFrame();
}

void UVoxelAnimationComponent::SetAnimation(UVoxelAnimation* NewAnimation)
{
	Animation = NewAnimation;
	Targets.Reset();
	ResolvedAnimation = nullptr;
	ApplyFrame();
}

void UVoxelAnimationComponent::OnRegister()
{
	Super::OnRegister();
	ApplyFrame();
}

void UVoxelAnimationComponent::BeginPlay()
{
	Super::BeginPlay();
	ApplyFrame();
	if (bAutoPlay)
	{
		Play();
	}
}

void UVoxelAnimationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bPlaying || !Animation)
	{
		return;
	}

	const float First = static_cast<float>(Animation->FirstFrame);
	const float Length = static_cast<float>(Animation->GetNumFrames()); // every frame is shown for 1 / FramesPerSecond
	float NewFrame = Frame + DeltaTime * Animation->FramesPerSecond * PlayRate;

	if (bLoop)
	{
		NewFrame = First + FMath::Fmod(NewFrame - First, Length);
		if (NewFrame < First)
		{
			NewFrame += Length; // playing backwards
		}
	}
	else if (NewFrame >= First + Length || NewFrame < First)
	{
		if (IsEditorPreview())
		{
			// The preview keeps playing until Stop, even without Loop.
			NewFrame = NewFrame < First ? First + Length - 1.0f : First;
			Frame = NewFrame;
			ApplyFrame();
			return;
		}
		NewFrame = FMath::Clamp(NewFrame, First, First + Length - 1.0f);
		Frame = NewFrame;
		ApplyFrame();
		bPlaying = false;
		SetComponentTickEnabled(false);
		OnFinished.Broadcast();
		return;
	}

	Frame = NewFrame;
	ApplyFrame();
}

#if WITH_EDITOR
void UVoxelAnimationComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	Targets.Reset();
	ResolvedAnimation = nullptr;
	ApplyFrame();
}
#endif

USceneComponent* UVoxelAnimationComponent::FindTarget(FName ComponentName) const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	TInlineComponentArray<USceneComponent*> Components(Owner);
	if (ComponentName.IsNone())
	{
		// The root component if it shows a model, otherwise the first component that does.
		if (VoxelAnimationComponentPrivate::ShowsModel(Owner->GetRootComponent()))
		{
			return Owner->GetRootComponent();
		}
		for (USceneComponent* Component : Components)
		{
			if (VoxelAnimationComponentPrivate::ShowsModel(Component))
			{
				return Component;
			}
		}
		return Owner->GetRootComponent();
	}

	for (USceneComponent* Component : Components)
	{
		if (Component && Component->GetFName() == ComponentName)
		{
			return Component;
		}
	}
	return nullptr;
}

void UVoxelAnimationComponent::ResolveTargets()
{
	Targets.Reset();
	ResolvedAnimation = Animation.Get();
	if (!Animation)
	{
		return;
	}

	Targets.Reserve(Animation->Tracks.Num());
	for (const FVoxelAnimationTrack& Track : Animation->Tracks)
	{
		USceneComponent* Target = FindTarget(Track.ComponentName);
		// Moving a component or changing its mesh at runtime needs a movable component.
		if (Target && Target->Mobility != EComponentMobility::Movable)
		{
			Target->SetMobility(EComponentMobility::Movable);
		}
		Targets.Add(Target);
	}
}

void UVoxelAnimationComponent::ApplyFrame()
{
	if (!Animation)
	{
		return;
	}
	if (ResolvedAnimation.Get() != Animation.Get() || Targets.Num() != Animation->Tracks.Num())
	{
		ResolveTargets();
	}

	for (int32 TrackIndex = 0; TrackIndex < Animation->Tracks.Num(); ++TrackIndex)
	{
		USceneComponent* Target = Targets.IsValidIndex(TrackIndex) ? Targets[TrackIndex].Get() : nullptr;
		if (!Target)
		{
			continue;
		}

		bool bHasTransform = false;
		bool bHasModel = false;
		FTransform Transform;
		UObject* Model = nullptr;
		if (!Animation->SampleTrack(TrackIndex, Frame, bHasTransform, Transform, bHasModel, Model))
		{
			continue;
		}

		if (bHasTransform && !Target->GetRelativeTransform().Equals(Transform))
		{
			Target->SetRelativeTransform(Transform);
		}
		if (!bHasModel)
		{
			continue;
		}
		if (UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Target))
		{
			UStaticMesh* Mesh = Cast<UStaticMesh>(Model);
			if (MeshComponent->GetStaticMesh() != Mesh)
			{
				MeshComponent->SetStaticMesh(Mesh);
			}
		}
		else if (UVoxelComponent* VoxelComponent = Cast<UVoxelComponent>(Target))
		{
			UVoxel* Voxel = Cast<UVoxel>(Model);
			if (VoxelComponent->GetVoxel() != Voxel)
			{
				VoxelComponent->SetVoxel(Voxel);
			}
		}
	}
}
