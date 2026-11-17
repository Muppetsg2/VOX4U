// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "VoxelFactoryNew.generated.h"

/**
 * Content Browser > Add > Miscellaneous > Voxel: a new voxel asset with one voxel of one colour (the engine's
 * cube), to be built in the Voxel Editor.
 */
UCLASS()
class UVoxelFactoryNew : public UFactory
{
	GENERATED_BODY()

public:

	UVoxelFactoryNew(const FObjectInitializer& ObjectInitializer);

	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;

	virtual bool ShouldShowInNewMenu() const override;

	virtual FText GetDisplayName() const override;

	virtual FText GetToolTip() const override;
};
