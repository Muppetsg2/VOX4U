// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Templates/Function.h"
#include "UObject/ObjectPtr.h"
#include "VoxelAnimationComponent.generated.h"

class USceneComponent;
class UVoxelAnimation;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnVoxelAnimationFinished);

/**
 * Plays a UVoxelAnimation on the components of its actor (found by name), e.g. on the components of a
 * scene Blueprint imported from a .vox file: transforms of any scene component, models of static mesh
 * components (static meshes) and voxel components (voxel assets).
 *
 * In the editor the Play / Stop buttons of the details panel preview the animation in the level or in the
 * Blueprint editor's viewport (Stop returns to the frame it started from); Frame shows any single frame and can
 * be keyed in Sequencer.
 */
UCLASS(ClassGroup = Animation, Meta = (BlueprintSpawnableComponent))
class VOX4U_API UVoxelAnimationComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UVoxelAnimationComponent();

	/** Animation to play. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Animation", Meta = (DisplayName = "Animation", ToolTip = "Animation to play."))
	TObjectPtr<UVoxelAnimation> Animation;

	/** Start playing when the game starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel Animation", Meta = (DisplayName = "Auto Play", ToolTip = "Start playing when the game starts."))
	bool bAutoPlay;

	/** Start again from the first frame after the last one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel Animation", Meta = (DisplayName = "Loop", ToolTip = "Start again from the first frame after the last one."))
	bool bLoop;

	/** Speed multiplier; negative values play backwards. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Voxel Animation", Meta = (DisplayName = "Play Rate", ToolTip = "Speed multiplier of the animation's frames per second; negative values play backwards."))
	float PlayRate;

	/** Current frame (MagicaVoxel frame number). Shown in the editor, can be keyed in Sequencer. */
	UPROPERTY(EditAnywhere, Interp, BlueprintReadOnly, Category = "Voxel Animation", Meta = (DisplayName = "Frame", ToolTip = "Current frame (MagicaVoxel frame number). Changing it in the editor previews the frame; it can be keyed in Sequencer."))
	float Frame;

	/** Called when a non-looping animation reaches its end. */
	UPROPERTY(BlueprintAssignable, Category = "Voxel Animation")
	FOnVoxelAnimationFinished OnFinished;

	/** Plays from the current frame. In the editor: previews the animation in the level. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Voxel Animation", Meta = (DisplayName = "Play", ToolTip = "Plays the animation from the current frame. In the editor: previews it in the level."))
	void Play();

	/** Stops at the current frame. In the editor: ends the preview and returns to the frame it started from. */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Voxel Animation", Meta = (DisplayName = "Stop", ToolTip = "Stops the animation at the current frame. In the editor: ends the preview and returns to the frame it started from."))
	void Stop();

	UFUNCTION(BlueprintPure, Category = "Voxel Animation")
	bool IsPlaying() const;

	/** Jumps to a frame and shows it. Also used by Sequencer to animate Frame. */
	UFUNCTION(BlueprintCallable, Category = "Voxel Animation")
	void SetFrame(float NewFrame);

	/** Changes the animation and shows its current frame. */
	UFUNCTION(BlueprintCallable, Category = "Voxel Animation")
	void SetAnimation(UVoxelAnimation* NewAnimation);

	//~ UActorComponent
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:

	/** Shows Frame on the target components. */
	void ApplyFrame();

	/** Finds the component of every track. */
	void ResolveTargets();

	USceneComponent* FindTarget(FName ComponentName) const;

	/** True outside of a game: editor level, Blueprint editor. */
	bool IsEditorPreview() const;

#if WITH_EDITOR
	/**
	 * The details panel of the Blueprint editor shows the component template, which is not in any world: Play and
	 * Stop pressed there act on the components made from it in the Blueprint editor's viewport.
	 */
	void ForEachPreviewInstance(TFunctionRef<void(UVoxelAnimationComponent&)> Action);
#endif

	TArray<TWeakObjectPtr<USceneComponent>> Targets;
	TWeakObjectPtr<UVoxelAnimation> ResolvedAnimation;
	bool bPlaying;

	/** Frame the editor preview started from. */
	float PreviewStartFrame;
};
