// Copyright 2016-2018 mik14a / Admix Network. All Rights Reserved.
// Edited by Muppetsg2 2025-2026

#include "VoxelFactory.h"
#include "AssetImportTask.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ObjectTools.h"
#include "Subsystems/ImportSubsystem.h"
#include "VoxAssetImportData.h"
#include "VoxAssetUtils.h"
#include "VoxImportOption.h"
#include "VoxImporter.h"
#include "Voxel.h"

DEFINE_LOG_CATEGORY_STATIC(LogVoxelFactory, Log, All)

#define LOCTEXT_NAMESPACE "VoxelFactory"

UVoxelFactory::UVoxelFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ImportOption(nullptr)
	, bShowOption(true)
	, bImportAllChosen(false)
{
	Formats.Add(TEXT("vox;MagicaVoxel"));

	SupportedClass = UStaticMesh::StaticClass();
	bCreateNew = false;
	bText = false;
	bEditorImport = true;
}

void UVoxelFactory::PostInitProperties()
{
	Super::PostInitProperties();
	ImportOption = NewObject<UVoxImportOption>(this, NAME_None, RF_NoFlags);
	// A new object copies the class defaults, which hold the options saved when the editor started; the options
	// saved by later imports (SaveConfig) are read here, so the dialog shows the last choice.
	ImportOption->LoadConfig();
}

bool UVoxelFactory::DoesSupportClass(UClass* Class)
{
	return Class == UStaticMesh::StaticClass()
		|| Class == UVoxel::StaticClass()
		|| Class == UBlueprint::StaticClass();
}

UVoxImportOption* UVoxelFactory::GetActiveOptions() const
{
	// Options given by a script (AssetImportTask.options) win over the ones saved for the dialog.
	if (AssetImportTask)
	{
		if (UVoxImportOption* TaskOptions = Cast<UVoxImportOption>(AssetImportTask->Options))
		{
			return TaskOptions;
		}
	}
	return ImportOption;
}

UClass* UVoxelFactory::ResolveSupportedClass()
{
	switch (GetActiveOptions()->VoxImportType)
	{
		case EVoxImportType::Voxel: return UVoxel::StaticClass();
		case EVoxImportType::Scene: return UBlueprint::StaticClass();
		case EVoxImportType::StaticMesh:
		default:                    return UStaticMesh::StaticClass();
	}
}

UObject* UVoxelFactory::FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn)
{
	UImportSubsystem* ImportSubsystem = GEditor ? GEditor->GetEditorSubsystem<UImportSubsystem>() : nullptr;
	if (ImportSubsystem)
	{
		ImportSubsystem->OnAssetPreImport.Broadcast(this, InClass, InParent, InName, Type);
	}

	// Options from a script (AssetImportTask.options) or the ones saved for this project.
	UVoxImportOption* Options = GetActiveOptions();
	const bool bScriptOptions = Options != ImportOption;

	// No dialog for automated imports (AssetImportTask.automated, commandlets, -unattended).
	const bool bInteractive = bShowOption && !IsAutomatedImport() && !FApp::IsUnattended();
	if (bInteractive)
	{
		bool bImportAll = false;
		const bool bImport = Options->GetImportOption(bImportAll);
		if (!bScriptOptions)
		{
			ImportOption->SaveConfig();
		}
		if (!bImport)
		{
			UE_LOG(LogVoxelFactory, Log, TEXT("Vox import canceled."));
			if (ImportSubsystem)
			{
				ImportSubsystem->OnAssetPostImport.Broadcast(this, nullptr);
			}
			return nullptr;
		}
		bShowOption = !bImportAll;
		bImportAllChosen |= bImportAll;
	}

	const FVoxImportSettings Settings = Options->ToSettings();

	const FString BaseName = MakeBaseName(*Options, InName);

	const FString PackagePath = FPackageName::GetLongPackagePath(InParent->GetOutermost()->GetName());
	const FVoxAssetLocation Location = FVoxAssetLocation::Make(PackagePath, BaseName, Settings, Flags);

	FVoxImporter Importer(Settings, Location, UFactory::GetCurrentFilename());
	const TArray<UObject*> Results = Importer.Import(Buffer, BufferEnd - Buffer);
	if (Results.Num() == 0 && Warn)
	{
		const FText Message = Importer.GetError().IsEmpty() ? LOCTEXT("NothingImported", "Nothing was imported.") : Importer.GetError();
		Warn->Logf(ELogVerbosity::Error, TEXT("VOX4U: %s"), *Message.ToString());
	}

	UObject* MainAsset = Results.Num() > 0 ? Results[0] : nullptr;
	if (ImportSubsystem)
	{
		ImportSubsystem->OnAssetPostImport.Broadcast(this, MainAsset);
	}
	return MainAsset;
}

FString UVoxelFactory::MakeBaseName(const UVoxImportOption& Options, FName FileName)
{
	const FString CustomName = ObjectTools::SanitizeObjectName(Options.CustomAssetName);
	if (CustomName.IsEmpty())
	{
		return FileName.ToString();
	}

	int32& Uses = CustomNameUses.FindOrAdd(CustomName);
	++Uses;
	const FString BaseName = bImportAllChosen || Uses > 1 ? FString::Printf(TEXT("%s_%d"), *CustomName, Uses) : CustomName;
	UE_LOG(LogVoxelFactory, Log, TEXT("Custom asset name: %s"), *BaseName);
	return BaseName;
}

void UVoxelFactory::CleanUp()
{
	Super::CleanUp();
	bShowOption = true;
	bImportAllChosen = false;
	CustomNameUses.Reset();
}

UVoxAssetImportData* UVoxelFactory::GetVoxImportData(UObject* Obj)
{
	if (UStaticMesh* StaticMesh = Cast<UStaticMesh>(Obj))
	{
		return Cast<UVoxAssetImportData>(StaticMesh->GetAssetImportData());
	}
	if (UVoxel* Voxel = Cast<UVoxel>(Obj))
	{
		return Cast<UVoxAssetImportData>(Voxel->GetAssetImportData());
	}
	return nullptr;
}

bool UVoxelFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	UVoxAssetImportData* AssetImportData = GetVoxImportData(Obj);
	if (AssetImportData && FPaths::GetExtension(AssetImportData->GetFirstFilename()).Equals(TEXT("vox"), ESearchCase::IgnoreCase))
	{
		AssetImportData->ExtractFilenames(OutFilenames);
		return true;
	}
	return false;
}

void UVoxelFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UVoxAssetImportData* AssetImportData = GetVoxImportData(Obj);
	if (AssetImportData && ensure(NewReimportPaths.Num() == 1))
	{
		AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}
}

EReimportResult::Type UVoxelFactory::Reimport(UObject* Obj)
{
	UVoxAssetImportData* AssetImportData = GetVoxImportData(Obj);
	if (!AssetImportData)
	{
		return EReimportResult::Failed;
	}

	const FString Filename = AssetImportData->GetFirstFilename();
	if (!Filename.Len() || IFileManager::Get().FileSize(*Filename) == INDEX_NONE)
	{
		UE_LOG(LogVoxelFactory, Warning, TEXT("Reimport failed: source file %s not found."), *Filename);
		return EReimportResult::Failed;
	}

	TArray<uint8> Data;
	if (!FFileHelper::LoadFileToArray(Data, *Filename))
	{
		UE_LOG(LogVoxelFactory, Warning, TEXT("Reimport failed: could not read %s."), *Filename);
		return EReimportResult::Failed;
	}

	// The asset is rebuilt in place with the options it was imported with; materials and textures are
	// found again by their names (derived from BaseName) and updated.
	const FVoxImportSettings Settings = AssetImportData->ToSettings();
	const FString BaseName = AssetImportData->BaseName.IsEmpty() ? FPaths::GetBaseFilename(Filename) : AssetImportData->BaseName;
	const FString PackagePath = FPackageName::GetLongPackagePath(Obj->GetOutermost()->GetName());
	const FVoxAssetLocation Location = FVoxAssetLocation::Make(PackagePath, BaseName, Settings, RF_Public | RF_Standalone | RF_Transactional);

	FVoxImporter Importer(Settings, Location, Filename);
	if (!Importer.Reimport(Obj, AssetImportData->ModelIndex, AssetImportData->AnimationFrame, Data.GetData(), Data.Num()))
	{
		UE_LOG(LogVoxelFactory, Warning, TEXT("Reimport failed: %s"), *Importer.GetError().ToString());
		return EReimportResult::Failed;
	}

	UE_LOG(LogVoxelFactory, Log, TEXT("Reimported %s from %s."), *Obj->GetName(), *Filename);
	Obj->MarkPackageDirty();
	return EReimportResult::Succeeded;
}

#undef LOCTEXT_NAMESPACE
