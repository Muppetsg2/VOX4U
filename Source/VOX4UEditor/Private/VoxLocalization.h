// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"

/** One translated message of a .po file. */
struct FVoxPoEntry
{
	/** msgctxt: "Namespace,Key" */
	FString Context;
	/** msgid: the English source text */
	FString Source;
	/** msgstr: the translation */
	FString Translation;
};

/**
 * Translations of the plugin's user interface.
 *
 * Every text of the plugin (LOCTEXT, property / enum display names and tooltips) is looked up in
 * Plugins/VOX4U/Localization/<culture>.po for the editor language (e.g. pl.po, es.po, pt-BR.po; a
 * language falls back to its parent, pt-BR -> pt). Dropping such a file into that folder is enough:
 * no compilation, no Localization Dashboard. The messages are loaded at startup and again whenever
 * the editor language changes. Localization/VOX4U.pot lists every message.
 *
 * The keys are the ones Unreal uses itself ("UObjectDisplayNames,VoxImportOption:Scale", "VoxImporter,
 * ReadingFile", ...), so the same files also work with Unreal's own localization tools.
 */
class FVoxLocalization
{
public:

	void Startup();
	void Shutdown();

	/** Parses the text of a .po file. Fuzzy and untranslated messages are skipped. */
	static void ParsePo(const FString& Text, TArray<FVoxPoEntry>& OutEntries);

	/** Folder with the .po files. */
	static FString GetLocalizationDir();

private:

	/** Loads the .po files of the current editor language into the text tables. */
	void Apply();

	void OnTextRevisionChanged();

	FDelegateHandle TextRevisionChangedHandle;
	bool bApplying = false;
	bool bApplied = false;
	uint32 AppliedRevision = 0;
};
