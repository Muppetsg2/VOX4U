// Copyright 2016-2018 mik14a / Admix Network. All Rights Reserved.
// Edited by Muppetsg2 2025-2026

#include "VoxelComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "Voxel.h"

namespace VoxelComponentPrivate
{
	const FIntVector NeighbourDirections[6] = {
		FIntVector(+0, +0, +1),	// Up
		FIntVector(+0, +0, -1),	// Down
		FIntVector(+1, +0, +0),	// Forward
		FIntVector(-1, +0, +0),	// Backward
		FIntVector(+0, +1, +0),	// Right
		FIntVector(+0, -1, +0)	// Left
	};
}

UVoxelComponent::UVoxelComponent()
	: CellBounds(FVector::ZeroVector, FVector(100.f, 100.f, 100.f), 100.f)
	, bHideUnbeheld(true)
	, Meshes()
	, Cells()
	, Voxel(nullptr)
	, InstancedStaticMeshComponents()
{
}

#if WITH_EDITOR
void UVoxelComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	static const FName NAME_HideUnbeheld = GET_MEMBER_NAME_CHECKED(UVoxelComponent, bHideUnbeheld);
	static const FName NAME_Meshes = GET_MEMBER_NAME_CHECKED(UVoxelComponent, Meshes);
	static const FName NAME_Voxel = GET_MEMBER_NAME_CHECKED(UVoxelComponent, Voxel);
	if (PropertyChangedEvent.Property)
	{
		const FName PropertyName = PropertyChangedEvent.Property->GetFName();
		if (PropertyName == NAME_HideUnbeheld)
		{
			ClearVoxel();
			AddVoxel();
		}
		else if (PropertyName == NAME_Meshes)
		{
			FBoxSphereBounds MeshesBounds(ForceInit);
			bool bFirst = true;
			for (int32 i = 0; i < Meshes.Num() && i < InstancedStaticMeshComponents.Num(); ++i)
			{
				if (InstancedStaticMeshComponents[i])
				{
					InstancedStaticMeshComponents[i]->SetStaticMesh(Meshes[i]);
				}
				if (Meshes[i])
				{
					MeshesBounds = bFirst ? Meshes[i]->GetBounds() : MeshesBounds + Meshes[i]->GetBounds();
					bFirst = false;
				}
			}
			CellBounds = MeshesBounds;
		}
		else if (PropertyName == NAME_Voxel)
		{
			SetVoxel(Voxel, true);
		}
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif // WITH_EDITOR

void UVoxelComponent::SetVoxel(class UVoxel* InVoxel, bool bForce /*= false*/)
{
	if (Voxel != InVoxel || bForce)
	{
		Voxel = InVoxel;
		// A template (Blueprint default) only stores the asset; instances build their cubes when registered.
		if (!IsTemplate())
		{
			InitVoxel();
		}
	}
}

const UVoxel* UVoxelComponent::GetVoxel() const
{
	return Voxel;
}

void UVoxelComponent::OnRegister()
{
	Super::OnRegister();

	if (Voxel && InstancedStaticMeshComponents.Num() == 0)
	{
		// e.g. a component added in a Blueprint with a default Voxel: the proxies were never created.
		InitVoxel();
		return;
	}

	// Proxies created while this component was unregistered (SetVoxel between Unregister / Register).
	if (GetWorld())
	{
		for (UInstancedStaticMeshComponent* Proxy : InstancedStaticMeshComponents)
		{
			if (Proxy && !Proxy->IsRegistered())
			{
				Proxy->RegisterComponent();
			}
		}
	}
}

void UVoxelComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	DestroyInstancedComponents();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UVoxelComponent::DestroyInstancedComponents()
{
	for (UInstancedStaticMeshComponent* Proxy : InstancedStaticMeshComponents)
	{
		if (Proxy)
		{
			Proxy->DestroyComponent();
		}
	}
	InstancedStaticMeshComponents.Empty();
}

void UVoxelComponent::InitVoxel()
{
	DestroyInstancedComponents();

	CellBounds = FBoxSphereBounds(FVector::ZeroVector, FVector(100.f, 100.f, 100.f), 100.f);
	Meshes.Empty();
	Cells.Empty();
	if (!Voxel)
	{
		return;
	}

	CellBounds = Voxel->CellBounds;
	Meshes = Voxel->Meshes;
	Cells = Voxel->Voxels;

	// The instanced cubes follow this component (e.g. a node of a scene Blueprint) and are rebuilt on register,
	// so they are never saved.
	for (int32 i = 0; i < Meshes.Num(); ++i)
	{
		UInstancedStaticMeshComponent* Proxy = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
		Proxy->SetMobility(Mobility);
		Proxy->SetStaticMesh(Meshes[i]);
		Proxy->SetupAttachment(this);
		if (IsRegistered() && GetWorld())
		{
			Proxy->RegisterComponent();
		}
		InstancedStaticMeshComponents.Add(Proxy);
	}
	AddVoxel();
}

FVector UVoxelComponent::GetCellTranslation(const FIntVector& InVector) const
{
	const FVector CellSize = CellBounds.BoxExtent * 2;
	FVector Offset = FVector::ZeroVector;
	if (Voxel && Voxel->bUsePivot)
	{
		Offset = Voxel->Pivot * CellSize; // the pivot cell corner lies at the origin
	}
	else if (Voxel && Voxel->bXYCenter)
	{
		Offset = FVector((float)Voxel->Size.X, (float)Voxel->Size.Y, 0.f) * CellBounds.BoxExtent;
	}
	return FVector(InVector) * CellSize - CellBounds.Origin + CellBounds.BoxExtent - Offset;
}

void UVoxelComponent::AddVoxel()
{
	if (!Voxel)
	{
		return;
	}
	// Instances are collected per mesh and added in one call each (one render state update per mesh).
	TArray<TArray<FTransform>> Instances;
	Instances.SetNum(InstancedStaticMeshComponents.Num());
	for (const TPair<FIntVector, uint8>& Cell : Voxel->Voxels)
	{
		if (!Instances.IsValidIndex(Cell.Value))
		{
			continue;
		}
		if (bHideUnbeheld && IsUnbeheldVolume(Cell.Key))
		{
			continue;
		}
		Instances[Cell.Value].Add(FTransform(FQuat::Identity, GetCellTranslation(Cell.Key), FVector(1.f)));
	}

	for (int32 MeshIndex = 0; MeshIndex < Instances.Num(); ++MeshIndex)
	{
		UInstancedStaticMeshComponent* Proxy = InstancedStaticMeshComponents[MeshIndex];
		if (Proxy && Instances[MeshIndex].Num() > 0)
		{
			Proxy->AddInstances(Instances[MeshIndex], false);
		}
	}
}

void UVoxelComponent::ClearVoxel()
{
	for (UInstancedStaticMeshComponent* Proxy : InstancedStaticMeshComponents)
	{
		if (Proxy)
		{
			Proxy->ClearInstances();
		}
	}
}

bool UVoxelComponent::IsUnbeheldVolume(const FIntVector& InVector) const
{
	if (!Voxel)
	{
		return false;
	}
	for (const FIntVector& Direction : VoxelComponentPrivate::NeighbourDirections)
	{
		if (!Voxel->Voxels.Contains(InVector + Direction))
		{
			return false;
		}
	}
	return true;
}

bool UVoxelComponent::GetVoxelTransform(const FIntVector& InVector, FTransform& OutVoxelTransform, bool bWorldSpace /*= false*/) const
{
	if (!Voxel || !Cells.Contains(InVector))
	{
		return false;
	}
	OutVoxelTransform = FTransform(FQuat::Identity, GetCellTranslation(InVector), FVector(1.f));
	if (bWorldSpace)
	{
		OutVoxelTransform = OutVoxelTransform * GetComponentToWorld();
	}
	return true;
}

FBoxSphereBounds UVoxelComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBoxSphereBounds ComponentBounds(ForceInit);
	bool bFirst = true;
	for (UInstancedStaticMeshComponent* Proxy : InstancedStaticMeshComponents)
	{
		if (!Proxy)
		{
			continue;
		}
		const FBoxSphereBounds ProxyBounds = Proxy->CalcBounds(LocalToWorld);
		ComponentBounds = bFirst ? ProxyBounds : ComponentBounds + ProxyBounds;
		bFirst = false;
	}
	return bFirst ? FBoxSphereBounds(LocalToWorld.GetLocation(), FVector::ZeroVector, 0.f) : ComponentBounds;
}

const TArray<TObjectPtr<UInstancedStaticMeshComponent>>& UVoxelComponent::GetInstancedStaticMeshComponent() const
{
	return InstancedStaticMeshComponents;
}
