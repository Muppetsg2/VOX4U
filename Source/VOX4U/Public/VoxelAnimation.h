// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "VoxelAnimation.generated.h"

/** Animation of one component: its transform and / or model in every frame of the animation. */
USTRUCT(BlueprintType)
struct VOX4U_API FVoxelAnimationTrack
{
	GENERATED_BODY()

	/** Name of the animated component of the actor. None: the root component (or the first one that shows a model). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Animation", Meta = (DisplayName = "Component Name", ToolTip = "Name of the animated component of the actor. None: the root component, or the first static mesh or voxel component."))
	FName ComponentName;

	/** Relative transform in every frame. Empty: the transform is not animated. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Animation", Meta = (DisplayName = "Transforms", ToolTip = "Relative transform of the component in every frame. Empty: the transform is not animated."))
	TArray<FTransform> Transforms;

	/** Index into UVoxelAnimation::Models in every frame, -1 for none. Empty: the model is not animated. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Animation", Meta = (DisplayName = "Model Indices", ToolTip = "Model shown in every frame (index into Models, -1 for none). Empty: the model is not animated."))
	TArray<int32> ModelIndices;
};

/**
 * A MagicaVoxel animation baked frame by frame: node transforms (keyframe animation) and the models
 * shown by them (frame-by-frame animation). Played by UVoxelAnimationComponent.
 */
UCLASS(BlueprintType)
class VOX4U_API UVoxelAnimation : public UObject
{
	GENERATED_BODY()

public:

	UVoxelAnimation();

	/** First frame of the animation range (MagicaVoxel's timeline). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel Animation", Meta = (DisplayName = "First Frame", ToolTip = "First frame of the animation range of MagicaVoxel's timeline."))
	int32 FirstFrame;

	/** Last frame of the animation range (MagicaVoxel's timeline). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voxel Animation", Meta = (DisplayName = "Last Frame", ToolTip = "Last frame of the animation range of MagicaVoxel's timeline."))
	int32 LastFrame;

	/** Playback speed. MagicaVoxel does not store it in the file. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Animation", Meta = (DisplayName = "Frames Per Second", ClampMin = "0.01", UIMin = "1", UIMax = "60", ToolTip = "Playback speed. MagicaVoxel does not store it in the file."))
	float FramesPerSecond;

	/** Interpolate transforms between frames (smooth sampling). Off: every frame is held, as in MagicaVoxel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Animation", Meta = (DisplayName = "Interpolate", ToolTip = "Interpolate the transforms between frames (translation and rotation). Off: every frame is held, as in MagicaVoxel. Models and mirroring always switch at whole frames."))
	bool bInterpolate;

	/** Models the tracks switch between: static meshes (static mesh components) or voxel assets (voxel components). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Animation", Meta = (DisplayName = "Models", AllowedClasses = "/Script/Engine.StaticMesh,/Script/VOX4U.Voxel", ToolTip = "Models the tracks switch between: static meshes for static mesh components, voxel assets for voxel components."))
	TArray<TObjectPtr<UObject>> Models;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Voxel Animation", Meta = (DisplayName = "Tracks", ToolTip = "Animated components."))
	TArray<FVoxelAnimationTrack> Tracks;

	/** Number of frames of the animation range. */
	UFUNCTION(BlueprintPure, Category = "Voxel Animation")
	int32 GetNumFrames() const;

	/** Length of one pass of the animation in seconds. */
	UFUNCTION(BlueprintPure, Category = "Voxel Animation")
	float GetDuration() const;

	/**
	 * State of a track at Frame (MagicaVoxel frame number, may be fractional).
	 * Returns which parts of the track are animated; the outputs are only written for those.
	 */
	bool SampleTrack(int32 TrackIndex, float Frame, bool& bOutHasTransform, FTransform& OutTransform, bool& bOutHasModel, UObject*& OutModel) const;
};
