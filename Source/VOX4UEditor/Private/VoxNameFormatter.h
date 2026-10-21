// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "VoxImportOption.h"
#include "VoxNamingSettings.h"

/** Values of the tokens of a name pattern. Negative numbers and empty strings count as "no value". */
struct FVoxNameFormatArgs
{
	FString BaseName;
	int32 ModelId = INDEX_NONE;
	int32 Color = INDEX_NONE;
	FString Description;
};

/** Builds asset names from the patterns in UVoxNamingSettings. */
class FVoxNameFormatter
{
public:

	/** Name of an asset of the given type, made a valid object name. */
	static FString Format(EVoxAssetType Type, const FVoxNameFormatArgs& Args, EVoxNamingConvention Convention);

	/**
	 * Replaces the tokens of Pattern: {BaseName}, {ModelId}, {Color} and {Description}, optionally with an
	 * underscore before and / or after the name ({_Color_}) that is only written when the token has a value.
	 * Unknown tokens are kept as they are.
	 */
	static FString ApplyPattern(const FString& Pattern, const FVoxNameFormatArgs& Args);
};
