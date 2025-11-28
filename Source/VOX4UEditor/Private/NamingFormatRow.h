// Copyright (c) 2025 Muppetsg2
// Licensed under the MIT License.

#pragma once

#include <CoreMinimal.h>
#include <Engine/DataTable.h>
#include "VoxImportOption.h"
#include "NameFormater.h"
#include "NamingFormatRow.generated.h"

USTRUCT(BlueprintType)
struct FNamingFormatRow : public FTableRowBase
{
    GENERATED_BODY()

public:

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Key")
    EFormaterObjectType ObjectType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Key")
    EVoxNamingConvention NamingConvention;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Value")
    FString Pattern;
};