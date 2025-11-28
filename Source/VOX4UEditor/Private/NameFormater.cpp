// Copyright (c) 2025 Muppetsg2
// Licensed under the MIT License.

#include "NameFormater.h"
#include "NamingFormatRow.h"
#include <Engine/DataTable.h>

DEFINE_LOG_CATEGORY_STATIC(VoxNameFormater, Log, All)

const UDataTable* NameFormater::CachedTable = nullptr;

FString NameFormater::GetFormatedName(const EFormaterObjectType Type, const NameFormatArgs& Args, const EVoxNamingConvention Convention)
{
    return ApplyPattern(GetPattern(Type, Convention), Args);
}

const UDataTable* NameFormater::GetNamingTable()
{
    if (CachedTable)
        return CachedTable;

    static const FString Path = TEXT("/VOX4U/Data/NamingFormats.NamingFormats");

    CachedTable = LoadObject<UDataTable>(nullptr, *Path);

    if (!CachedTable)
    {
        UE_LOG(VoxNameFormater, Error, TEXT("Failed to load NamingFormats DataTable at: %s"), *Path);
    }

    return CachedTable;
}

FString NameFormater::GetPattern(const EFormaterObjectType& Type, const EVoxNamingConvention& Convention)
{
    const UDataTable* Table = GetNamingTable();
    if (!Table)
        return "{BaseName}";
    
    FString TypeStr;
    switch (Type)
    {
        case EFormaterObjectType::Material:          TypeStr = TEXT("Material"); break;
        case EFormaterObjectType::MaterialInstance:  TypeStr = TEXT("MaterialInstance"); break;
        case EFormaterObjectType::Texture:           TypeStr = TEXT("Texture"); break;
        case EFormaterObjectType::StaticMesh:        TypeStr = TEXT("StaticMesh"); break;
        case EFormaterObjectType::Voxel:             TypeStr = TEXT("Voxel"); break;
    }

    FString ConvStr;
    switch (Convention)
    {
        case EVoxNamingConvention::UnrealEngine:    ConvStr = TEXT("UnrealEngine"); break;
        case EVoxNamingConvention::VOX4U:           ConvStr = TEXT("VOX4U"); break;
    }

    FString RowNameStr = FString::Printf(TEXT("%s%s"), *TypeStr, *ConvStr);
    const FNamingFormatRow* Row = Table->FindRow<FNamingFormatRow>(FName(*RowNameStr), TEXT(""));
    if (!Row)
    {
        UE_LOG(VoxNameFormater, Warning, TEXT("NameFormater: row '%s' not found"), *RowNameStr);
        return "{BaseName}";
    }
    
    return Row->Pattern;
}

FString NameFormater::ApplyPattern(const FString& Pattern, const NameFormatArgs& Args)
{
    FString Result = Pattern;
    
    // Default
    Result.ReplaceInline(TEXT("{BaseName}"), *Args.BaseName);
    Result.ReplaceInline(TEXT("{ModelId}"), Args.ModelId >= 0 ? *FString::FromInt(Args.ModelId) : TEXT(""));
    Result.ReplaceInline(TEXT("{Color}"), Args.Color >= 0 ? *FString::FromInt(Args.Color) : TEXT(""));
    Result.ReplaceInline(TEXT("{Description}"), *Args.Description);

    // _Pre
    Result.ReplaceInline(TEXT("{_BaseName}"), *FString::Printf(TEXT("_%s"), *Args.BaseName));
    Result.ReplaceInline(TEXT("{_ModelId}"), Args.ModelId >= 0 ? *FString::Printf(TEXT("_%d"), Args.ModelId) : TEXT(""));
    Result.ReplaceInline(TEXT("{_Color}"), Args.Color >= 0 ? *FString::Printf(TEXT("_%d"), Args.Color) : TEXT(""));
    Result.ReplaceInline(TEXT("{_Description}"), *FString::Printf(TEXT("_%s"), *Args.Description));

    // Post_
    Result.ReplaceInline(TEXT("{BaseName_}"), *FString::Printf(TEXT("%s_"), *Args.BaseName));
    Result.ReplaceInline(TEXT("{ModelId_}"), Args.ModelId >= 0 ? *FString::Printf(TEXT("%d_"), Args.ModelId) : TEXT(""));
    Result.ReplaceInline(TEXT("{Color_}"), Args.Color >= 0 ? *FString::Printf(TEXT("%d_"), Args.Color) : TEXT(""));
    Result.ReplaceInline(TEXT("{Description_}"), *FString::Printf(TEXT("%s_"), *Args.Description));

    // _Both_
    Result.ReplaceInline(TEXT("{_BaseName_}"), *FString::Printf(TEXT("_%s_"), *Args.BaseName));
    Result.ReplaceInline(TEXT("{_ModelId_}"), Args.ModelId >= 0 ? *FString::Printf(TEXT("_%d_"), Args.ModelId) : TEXT(""));
    Result.ReplaceInline(TEXT("{_Color_}"), Args.Color >= 0 ? *FString::Printf(TEXT("_%d_"), Args.Color) : TEXT(""));
    Result.ReplaceInline(TEXT("{_Description_}"), *FString::Printf(TEXT("_%s_"), *Args.Description));

    return Result;
}