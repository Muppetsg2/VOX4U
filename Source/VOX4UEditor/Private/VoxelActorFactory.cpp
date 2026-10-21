// Copyright 2016-2018 mik14a / Admix Network. All Rights Reserved.
// Edited by Muppetsg2 2025-2026

#include "VoxelActorFactory.h"
#include "AssetRegistry/AssetData.h"
#include "Voxel.h"
#include "VoxelActor.h"
#include "VoxelComponent.h"

#define LOCTEXT_NAMESPACE "VoxelActorFactory"

UVoxelActorFactory::UVoxelActorFactory(const FObjectInitializer& ObjectInitializer /*= FObjectInitializer::Get()*/)
	: Super(ObjectInitializer)
{
	DisplayName = LOCTEXT("DisplayName", "Voxel Actor");
	NewActorClass = AVoxelActor::StaticClass();
}

void UVoxelActorFactory::PostSpawnActor(UObject* Asset, AActor* NewActor)
{
	Super::PostSpawnActor(Asset, NewActor);

	AVoxelActor* VoxelActor = CastChecked<AVoxelActor>(NewActor);
	UVoxelComponent* VoxelComponent = VoxelActor->GetVoxelComponent();
	check(VoxelComponent);

	if (UVoxel* Voxel = Cast<UVoxel>(Asset))
	{
		VoxelComponent->UnregisterComponent();
		VoxelComponent->SetVoxel(Voxel, true);
		VoxelComponent->RegisterComponent();
	}
	VoxelActor->PostEditChange();
}

bool UVoxelActorFactory::CanCreateActorFrom(const FAssetData& AssetData, FText& OutErrorMsg)
{
	if (AssetData.IsValid())
	{
		UClass* AssetClass = AssetData.GetClass();
		if (AssetClass && AssetClass->IsChildOf(UVoxel::StaticClass()))
		{
			return true;
		} 
		else
		{
			OutErrorMsg = LOCTEXT("NoVoxel", "No Voxel data specified.");
			return false;
		}
	}
	else
	{
		return true;
	}
}

#undef LOCTEXT_NAMESPACE
