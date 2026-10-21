// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#include "VoxLocalization.h"
#include "Interfaces/IPluginManager.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Internationalization/TextLocalizationResource.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogVoxLocalization, Log, All)

namespace VoxLocalizationPrivate
{
	/** The text of a quoted .po string ("..." with C escapes), starting at the first quote of Line. */
	FString Unquote(const FString& Line)
	{
		int32 Start = INDEX_NONE;
		int32 End = INDEX_NONE;
		if (!Line.FindChar(TEXT('"'), Start) || !Line.FindLastChar(TEXT('"'), End) || End <= Start)
		{
			return FString();
		}

		FString Result;
		Result.Reserve(End - Start);
		for (int32 Index = Start + 1; Index < End; ++Index)
		{
			const TCHAR Char = Line[Index];
			if (Char != TEXT('\\') || Index + 1 >= End)
			{
				Result.AppendChar(Char);
				continue;
			}

			const TCHAR Escaped = Line[++Index];
			switch (Escaped)
			{
			case TEXT('n'):  Result.AppendChar(TEXT('\n')); break;
			case TEXT('t'):  Result.AppendChar(TEXT('\t')); break;
			case TEXT('r'):  Result.AppendChar(TEXT('\r')); break;
			case TEXT('"'):  Result.AppendChar(TEXT('"')); break;
			case TEXT('\\'): Result.AppendChar(TEXT('\\')); break;
			default:
				Result.AppendChar(TEXT('\\'));
				Result.AppendChar(Escaped);
				break;
			}
		}
		return Result;
	}
}

void FVoxLocalization::Startup()
{
	TextRevisionChangedHandle = FTextLocalizationManager::Get().OnTextRevisionChangedEvent.AddRaw(this, &FVoxLocalization::OnTextRevisionChanged);
	Apply();
}

void FVoxLocalization::Shutdown()
{
	if (TextRevisionChangedHandle.IsValid())
	{
		FTextLocalizationManager::Get().OnTextRevisionChangedEvent.Remove(TextRevisionChangedHandle);
		TextRevisionChangedHandle.Reset();
	}
}

FString FVoxLocalization::GetLocalizationDir()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("VOX4U"));
	return Plugin.IsValid() ? Plugin->GetBaseDir() / TEXT("Localization") : FString();
}

void FVoxLocalization::ParsePo(const FString& Text, TArray<FVoxPoEntry>& OutEntries)
{
	enum class EField : uint8 { None, Context, Source, Translation, Ignored };

	FVoxPoEntry Entry;
	EField Field = EField::None;
	bool bFuzzy = false;

	auto Flush = [&]()
	{
		if (!bFuzzy && !Entry.Context.IsEmpty() && !Entry.Translation.IsEmpty())
		{
			OutEntries.Add(Entry);
		}
		Entry = FVoxPoEntry();
		Field = EField::None;
		bFuzzy = false;
	};

	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines, false);
	for (FString& Line : Lines)
	{
		Line.TrimStartAndEndInline();

		// A blank line, or a comment / keyword after a msgstr, starts the next message.
		if (Line.IsEmpty())
		{
			Flush();
			continue;
		}
		const bool bStartsMessage = Line.StartsWith(TEXT("#")) || Line.StartsWith(TEXT("msgctxt")) || Line.StartsWith(TEXT("msgid "));
		if (Field == EField::Translation && bStartsMessage)
		{
			Flush();
		}

		if (Line.StartsWith(TEXT("#,")))
		{
			bFuzzy |= Line.Contains(TEXT("fuzzy"));
		}
		else if (Line.StartsWith(TEXT("#")))
		{
			// translator / extracted comments and references
		}
		else if (Line.StartsWith(TEXT("msgctxt")))
		{
			Entry.Context = VoxLocalizationPrivate::Unquote(Line);
			Field = EField::Context;
		}
		else if (Line.StartsWith(TEXT("msgid_plural")))
		{
			Field = EField::Ignored;
		}
		else if (Line.StartsWith(TEXT("msgid")))
		{
			Entry.Source = VoxLocalizationPrivate::Unquote(Line);
			Field = EField::Source;
		}
		else if (Line.StartsWith(TEXT("msgstr")))
		{
			Entry.Translation = VoxLocalizationPrivate::Unquote(Line);
			Field = EField::Translation;
		}
		else if (Line.StartsWith(TEXT("\"")))
		{
			// continuation of the previous string
			const FString Continuation = VoxLocalizationPrivate::Unquote(Line);
			switch (Field)
			{
			case EField::Context:     Entry.Context += Continuation; break;
			case EField::Source:      Entry.Source += Continuation; break;
			case EField::Translation: Entry.Translation += Continuation; break;
			default: break;
			}
		}
	}
	Flush();
}

void FVoxLocalization::Apply()
{
	const FString Directory = GetLocalizationDir();
	if (Directory.IsEmpty())
	{
		return;
	}

	// Most specific culture first (pt-BR, pt): its messages get the higher priority (lower value).
	const TArray<FString> Cultures = FInternationalization::Get().GetCurrentLanguage()->GetPrioritizedParentCultureNames();

	FTextLocalizationResource Resource;
	int32 NumMessages = 0;
	for (int32 Priority = 0; Priority < Cultures.Num(); ++Priority)
	{
		const FString File = Directory / Cultures[Priority] + TEXT(".po");
		FString Text;
		if (!FPaths::FileExists(File) || !FFileHelper::LoadFileToString(Text, *File))
		{
			continue;
		}

		TArray<FVoxPoEntry> Entries;
		ParsePo(Text, Entries);
		for (const FVoxPoEntry& Entry : Entries)
		{
			FString Namespace;
			FString Key;
			if (!Entry.Context.Split(TEXT(","), &Namespace, &Key, ESearchCase::CaseSensitive))
			{
				continue;
			}
			// The source text has to match the English text in the code, otherwise Unreal treats the
			// translation as out of date and keeps the English text.
			Resource.AddEntry(FTextKey(Namespace), FTextKey(Key), Entry.Source, Entry.Translation, Priority);
			++NumMessages;
		}
		UE_LOG(LogVoxLocalization, Log, TEXT("Loaded %d translated messages from %s"), Entries.Num(), *File);
	}

	if (NumMessages == 0)
	{
		bApplied = false; // e.g. English: the texts of the code are used
		return;
	}

	TGuardValue<bool> Guard(bApplying, true);
	FTextLocalizationManager::Get().UpdateFromLocalizationResource(Resource);
	AppliedRevision = FTextLocalizationManager::Get().GetTextRevision();
	bApplied = true;
}

void FVoxLocalization::OnTextRevisionChanged()
{
	if (bApplying)
	{
		return;
	}
	// The editor reloads every text table when its language changes, which drops our messages.
	if (bApplied && FTextLocalizationManager::Get().GetTextRevision() == AppliedRevision)
	{
		return;
	}
	Apply();
}
