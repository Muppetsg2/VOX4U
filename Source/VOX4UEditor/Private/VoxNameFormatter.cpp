// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#include "VoxNameFormatter.h"
#include "ObjectTools.h"

namespace VoxNameFormatterPrivate
{
	/** Value of a token name, empty when the token has no value. Returns false for an unknown token. */
	bool GetTokenValue(const FString& Token, const FVoxNameFormatArgs& Args, FString& OutValue)
	{
		if (Token == TEXT("BaseName"))
		{
			OutValue = Args.BaseName;
		}
		else if (Token == TEXT("ModelId"))
		{
			OutValue = Args.ModelId >= 0 ? FString::FromInt(Args.ModelId) : FString();
		}
		else if (Token == TEXT("Color"))
		{
			OutValue = Args.Color >= 0 ? FString::FromInt(Args.Color) : FString();
		}
		else if (Token == TEXT("Description"))
		{
			OutValue = Args.Description;
		}
		else
		{
			return false;
		}
		return true;
	}
}

FString FVoxNameFormatter::Format(EVoxAssetType Type, const FVoxNameFormatArgs& Args, EVoxNamingConvention Convention)
{
	FString Pattern = GetDefault<UVoxNamingSettings>()->GetPatterns(Convention).Get(Type);
	if (Pattern.TrimStartAndEnd().IsEmpty())
	{
		Pattern = FVoxNamingPatterns::MakeDefault(Convention).Get(Type);
	}

	FString Name = ApplyPattern(Pattern, Args);
	if (Name.IsEmpty())
	{
		Name = Args.BaseName;
	}
	return ObjectTools::SanitizeObjectName(Name);
}

FString FVoxNameFormatter::ApplyPattern(const FString& Pattern, const FVoxNameFormatArgs& Args)
{
	FString Result;
	Result.Reserve(Pattern.Len() + Args.BaseName.Len());

	int32 Index = 0;
	while (Index < Pattern.Len())
	{
		const TCHAR Char = Pattern[Index];
		int32 Close = INDEX_NONE;
		if (Char == TEXT('{'))
		{
			Close = Pattern.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Index + 1);
		}
		if (Close == INDEX_NONE)
		{
			Result.AppendChar(Char);
			++Index;
			continue;
		}

		// {_Name_}: optional separators around the token name
		FString Token = Pattern.Mid(Index + 1, Close - Index - 1);
		const bool bLeading = Token.StartsWith(TEXT("_"));
		const bool bTrailing = Token.Len() > 1 && Token.EndsWith(TEXT("_"));
		Token = Token.Mid(bLeading ? 1 : 0, Token.Len() - (bLeading ? 1 : 0) - (bTrailing ? 1 : 0));

		FString Value;
		if (!VoxNameFormatterPrivate::GetTokenValue(Token, Args, Value))
		{
			Result += Pattern.Mid(Index, Close - Index + 1); // unknown token, kept literally
		}
		else if (!Value.IsEmpty())
		{
			if (bLeading)
			{
				Result.AppendChar(TEXT('_'));
			}
			Result += Value;
			if (bTrailing)
			{
				Result.AppendChar(TEXT('_'));
			}
		}
		Index = Close + 1;
	}
	return Result;
}
