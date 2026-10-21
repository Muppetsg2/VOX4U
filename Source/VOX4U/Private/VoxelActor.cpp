// Copyright 2016-2018 mik14a / Admix Network. All Rights Reserved.
// Edited by Muppetsg2 2025-2026

#include "VoxelActor.h"
#include "VoxelComponent.h"

AVoxelActor::AVoxelActor(const FObjectInitializer& ObjectInitializer /*= FObjectInitializer::Get()*/)
	: Super(ObjectInitializer)
{
	VoxelComponent = CreateDefaultSubobject<UVoxelComponent>(TEXT("VoxelComponent"));
	// The actor had no root component before, so it could not be moved, rotated or scaled.
	RootComponent = VoxelComponent;
}

UVoxelComponent* AVoxelActor::GetVoxelComponent() const
{
	return VoxelComponent;
}
