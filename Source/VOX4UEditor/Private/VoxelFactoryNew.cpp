// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#include "VoxelFactoryNew.h"
#include "Engine/StaticMesh.h"
#include "Voxel.h"

#define LOCTEXT_NAMESPACE "VoxelFactoryNew"

UVoxelFactoryNew::UVoxelFactoryNew(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UVoxel::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
	bEditorImport = false;
}

UObject* UVoxelFactoryNew::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UVoxel* NewVoxel = NewObject<UVoxel>(InParent, InClass, InName, Flags | RF_Transactional);

	// The engine's cube (scaled to Scale); one colour, its material; one voxel standing on the origin.
	if (UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
	{
		NewVoxel->Mesh = Cube;
		NewVoxel->Materials.Add(Cube->GetMaterial(0));
		NewVoxel->Voxels.Add(FIntVector::ZeroValue, 0);
		NewVoxel->Size = FIntVector(1, 1, 1);
	}
	NewVoxel->Scale = 1.0f; // 1 cm voxels, as imported with the default Scale import option
	NewVoxel->bUsePivot = true;
	NewVoxel->Pivot = FVector(0.5, 0.5, 0.0);
	NewVoxel->CalcCellBounds();
	return NewVoxel;
}

bool UVoxelFactoryNew::ShouldShowInNewMenu() const
{
	return true;
}

FText UVoxelFactoryNew::GetDisplayName() const
{
	return LOCTEXT("DisplayName", "Voxel");
}

FText UVoxelFactoryNew::GetToolTip() const
{
	return LOCTEXT("ToolTip", "A new voxel asset to build in the Voxel Editor: add, remove and paint voxels with the mouse.");
}

#undef LOCTEXT_NAMESPACE
