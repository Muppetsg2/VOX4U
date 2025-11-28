// Copyright (c) 2025 Muppetsg2
// Licensed under the MIT License.

#pragma once

#include <CoreMinimal.h>
#include "VoxImportOption.h"

UENUM()
enum class EFormaterObjectType : uint8
{
    Material,
    MaterialInstance,
    Texture,
    StaticMesh,
    Voxel
};

struct NameFormatArgs
{
    FString BaseName;

    int32 Color = -1;
    int32 ModelId = -1;

    FString Description = "";
};

class NameFormater
{
public:

    static FString GetFormatedName(const EFormaterObjectType Type, const NameFormatArgs& Args, const EVoxNamingConvention Convention);

private:

    static const UDataTable* GetNamingTable();

    static FString GetPattern(const EFormaterObjectType& Type, const EVoxNamingConvention& Convention);

    static FString ApplyPattern(const FString& Pattern, const NameFormatArgs& Args);

private:

    static const UDataTable* CachedTable;
};