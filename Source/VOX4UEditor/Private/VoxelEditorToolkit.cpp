// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#include "VoxelEditorToolkit.h"
#include "Editor.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "SAdvancedPreviewDetailsTab.h"
#include "Subsystems/ImportSubsystem.h"
#include "SVoxelEditorViewport.h"
#include "UObject/UObjectGlobals.h"
#include "VOX4UVersion.h"
#include "Voxel.h"
#include "Widgets/Docking/SDockTab.h"
#if VOX4U_UE_AT_LEAST(5, 1)
#include "Styling/AppStyle.h"
#else
#include "EditorStyleSet.h"
#endif

#define LOCTEXT_NAMESPACE "VoxelEditor"

namespace VoxelEditorToolkitPrivate
{
	const FName AppIdentifier(TEXT("VoxelEditorApp"));
	const FName ViewportTabId(TEXT("VoxelEditor_Viewport"));
	const FName DetailsTabId(TEXT("VoxelEditor_Details"));
	const FName PreviewSettingsTabId(TEXT("VoxelEditor_PreviewSettings"));

	FSlateIcon TabIcon(const FName StyleName)
	{
#if VOX4U_UE_AT_LEAST(5, 1)
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), StyleName);
#else
		// UE 5.0: FAppStyle is not there yet.
		return FSlateIcon(FEditorStyle::GetStyleSetName(), StyleName);
#endif
	}
}

FVoxelEditorToolkit::~FVoxelEditorToolkit()
{
	FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(PropertyChangedHandle);
	if (GEditor)
	{
		if (UImportSubsystem* ImportSubsystem = GEditor->GetEditorSubsystem<UImportSubsystem>())
		{
			ImportSubsystem->OnAssetReimport.Remove(ReimportHandle);
		}
	}
}

void FVoxelEditorToolkit::Init(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UVoxel* InVoxel)
{
	using namespace VoxelEditorToolkitPrivate;

	EditedVoxel = InVoxel;

	VoxelViewport = SNew(SVoxelEditorViewport).Voxel(InVoxel);

	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	VoxelDetails = PropertyEditor.CreateDetailView(DetailsArgs);
	VoxelDetails->SetObject(InVoxel);

	PreviewSettings = SNew(SAdvancedPreviewDetailsTab, VoxelViewport->GetPreviewScene());

	PropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &FVoxelEditorToolkit::OnObjectPropertyChanged);
	if (GEditor)
	{
		if (UImportSubsystem* ImportSubsystem = GEditor->GetEditorSubsystem<UImportSubsystem>())
		{
			ReimportHandle = ImportSubsystem->OnAssetReimport.AddRaw(this, &FVoxelEditorToolkit::OnAssetReimport);
		}
	}

	const TSharedRef<FTabManager::FLayout> DefaultLayout = FTabManager::NewLayout("Standalone_VOX4U_VoxelEditor_v1")
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.7f)
				->SetHideTabWell(true)
				->AddTab(ViewportTabId, ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.3f)
				->AddTab(DetailsTabId, ETabState::OpenedTab)
				->AddTab(PreviewSettingsTabId, ETabState::OpenedTab)
				->SetForegroundTab(DetailsTabId)
			)
		);

	InitAssetEditor(Mode, InitToolkitHost, AppIdentifier, DefaultLayout, true, true, InVoxel);
}

void FVoxelEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	using namespace VoxelEditorToolkitPrivate;

	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu", "Voxel Editor"));
	const TSharedRef<FWorkspaceItem> Category = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(ViewportTabId, FOnSpawnTab::CreateSP(this, &FVoxelEditorToolkit::SpawnViewportTab))
		.SetDisplayName(LOCTEXT("ViewportTab", "Viewport"))
		.SetGroup(Category)
		.SetIcon(TabIcon("LevelEditor.Tabs.Viewports"));

	InTabManager->RegisterTabSpawner(DetailsTabId, FOnSpawnTab::CreateSP(this, &FVoxelEditorToolkit::SpawnDetailsTab))
		.SetDisplayName(LOCTEXT("DetailsTab", "Details"))
		.SetGroup(Category)
		.SetIcon(TabIcon("LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(PreviewSettingsTabId, FOnSpawnTab::CreateSP(this, &FVoxelEditorToolkit::SpawnPreviewSettingsTab))
		.SetDisplayName(LOCTEXT("PreviewSettingsTab", "Preview Scene Settings"))
		.SetGroup(Category)
		.SetIcon(TabIcon("LevelEditor.Tabs.Details"));
}

void FVoxelEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	using namespace VoxelEditorToolkitPrivate;

	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
	InTabManager->UnregisterTabSpawner(ViewportTabId);
	InTabManager->UnregisterTabSpawner(DetailsTabId);
	InTabManager->UnregisterTabSpawner(PreviewSettingsTabId);
}

FName FVoxelEditorToolkit::GetToolkitFName() const
{
	return FName(TEXT("VoxelEditor"));
}

FText FVoxelEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Voxel Editor");
}

FString FVoxelEditorToolkit::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Voxel ").ToString();
}

FLinearColor FVoxelEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.3f, 0.6f, 1.0f, 0.5f);
}

TSharedRef<SDockTab> FVoxelEditorToolkit::SpawnViewportTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("ViewportTab", "Viewport"))
		[
			VoxelViewport.ToSharedRef()
		];
}

TSharedRef<SDockTab> FVoxelEditorToolkit::SpawnDetailsTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("DetailsTab", "Details"))
		[
			VoxelDetails.ToSharedRef()
		];
}

TSharedRef<SDockTab> FVoxelEditorToolkit::SpawnPreviewSettingsTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.Label(LOCTEXT("PreviewSettingsTab", "Preview Scene Settings"))
		[
			PreviewSettings.ToSharedRef()
		];
}

void FVoxelEditorToolkit::OnObjectPropertyChanged(UObject* Object, FPropertyChangedEvent& Event)
{
	// Edits in the details panel and undo / redo. Dragging a value rebuilds once, when it is released.
	if (Object && Object == EditedVoxel.Get() && VoxelViewport.IsValid() && (Event.ChangeType & EPropertyChangeType::Interactive) == 0)
	{
		VoxelViewport->Refresh(false);
	}
}

void FVoxelEditorToolkit::OnAssetReimport(UObject* Object)
{
	if (Object && Object == EditedVoxel.Get() && VoxelViewport.IsValid())
	{
		VoxelViewport->Refresh(true);
	}
}

#undef LOCTEXT_NAMESPACE
