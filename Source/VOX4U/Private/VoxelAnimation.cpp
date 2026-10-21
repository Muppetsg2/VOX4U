// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#include "VoxelAnimation.h"

UVoxelAnimation::UVoxelAnimation()
	: FirstFrame(0)
	, LastFrame(0)
	, FramesPerSecond(10.0f)
	, bInterpolate(false)
{
}

int32 UVoxelAnimation::GetNumFrames() const
{
	return FMath::Max(1, LastFrame - FirstFrame + 1);
}

float UVoxelAnimation::GetDuration() const
{
	return static_cast<float>(GetNumFrames()) / FMath::Max(FramesPerSecond, 0.01f);
}

bool UVoxelAnimation::SampleTrack(int32 TrackIndex, float Frame, bool& bOutHasTransform, FTransform& OutTransform, bool& bOutHasModel, UObject*& OutModel) const
{
	bOutHasTransform = false;
	bOutHasModel = false;
	if (!Tracks.IsValidIndex(TrackIndex))
	{
		return false;
	}
	const FVoxelAnimationTrack& Track = Tracks[TrackIndex];

	// Frame within the range; the last frame is held until the end of its time slot.
	const float Local = FMath::Clamp(Frame - static_cast<float>(FirstFrame), 0.0f, static_cast<float>(GetNumFrames() - 1));
	const int32 Index = FMath::FloorToInt(Local);
	const float Alpha = Local - static_cast<float>(Index);

	if (Track.Transforms.Num() > 0)
	{
		const FTransform& A = Track.Transforms[FMath::Min(Index, Track.Transforms.Num() - 1)];
		OutTransform = A;
		if (bInterpolate && Alpha > 0.0f && Index + 1 < Track.Transforms.Num())
		{
			const FTransform& B = Track.Transforms[Index + 1];
			OutTransform.SetLocation(FMath::Lerp(A.GetLocation(), B.GetLocation(), static_cast<double>(Alpha)));
			// A mirrored and a not mirrored pose cannot be blended: the rotation is held like the mirroring.
			if (A.GetScale3D().Equals(B.GetScale3D()))
			{
				OutTransform.SetRotation(FQuat::Slerp(A.GetRotation(), B.GetRotation(), Alpha));
			}
		}
		bOutHasTransform = true;
	}

	if (Track.ModelIndices.Num() > 0)
	{
		const int32 ModelIndex = Track.ModelIndices[FMath::Min(Index, Track.ModelIndices.Num() - 1)];
		OutModel = Models.IsValidIndex(ModelIndex) ? Models[ModelIndex].Get() : nullptr;
		bOutHasModel = true;
	}

	return bOutHasTransform || bOutHasModel;
}
