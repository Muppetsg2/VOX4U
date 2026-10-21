// Copyright 2016-2018 mik14a / Admix Network. All Rights Reserved.
// Edited by Muppetsg2 2025-2026

#include "VoxImportOption.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IMainFrameModule.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SWindow.h"
#include "SVoxOptionWidget.h"

#define LOCTEXT_NAMESPACE "VoxImportOption"

UVoxImportOption::UVoxImportOption()
	: VoxImportType(EVoxImportType::StaticMesh)
	, AssetsNamingConvention(EVoxNamingConvention::UnrealEngine)
	, CustomAssetName(TEXT(""))
	, ResourcesSaveLocation(EVoxResourcesSaveLocation::SubFolder)
	, bImportXForward(true)
	, Pivot(EVoxPivot::BottomCenter)
	, bSeparateModels(false)
	, bImportHidden(false)
	, Frame(0)
	, Scale(1.0f)
	, bGenerateLightmapUVs(true)
	, bImportMaterial(true)
	, bOneMaterial(false)
	, bPaletteToTexture(false)
	, SceneModels(EVoxSceneModels::StaticMesh)
	, bImportAnimation(true)
	, AnimationSampling(EVoxAnimationSampling::MagicaVoxel)
	, AnimationFramesPerSecond(10.0f)
	, bImportSun(true)
	, bImportCameras(true)
	, LightIntensityScale(10.0f)
{
}

bool UVoxImportOption::GetImportOption(bool& bOutImportAll)
{
	TSharedPtr<SWindow> ParentWindow;

	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("WindowTitle", "VOX Import Options"))
		.SizingRule(ESizingRule::Autosized);

	TSharedPtr<SVoxOptionWidget> VoxOptionWidget;
	Window->SetContent(SAssignNew(VoxOptionWidget, SVoxOptionWidget).Window(Window).ImportOption(this));
	FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

	bOutImportAll = VoxOptionWidget->ShouldImportAll();
	return VoxOptionWidget->ShouldImport();
}

FVoxImportSettings UVoxImportOption::ToSettings() const
{
	FVoxImportSettings Settings;
	Settings.ImportType = VoxImportType;
	Settings.NamingConvention = AssetsNamingConvention;
	Settings.ResourcesSaveLocation = ResourcesSaveLocation;
	Settings.bImportXForward = bImportXForward;
	Settings.Pivot = Pivot;
	Settings.bSeparateModels = bSeparateModels;
	Settings.bImportHidden = bImportHidden;
	Settings.Frame = FMath::Max(0, Frame);
	Settings.Scale = FMath::Max(Scale, 0.001f);
	Settings.bGenerateLightmapUVs = bGenerateLightmapUVs;
	Settings.bImportMaterial = bImportMaterial;
	Settings.bOneMaterial = bOneMaterial;
	Settings.bPaletteToTexture = bPaletteToTexture;
	Settings.SceneModels = SceneModels;
	Settings.bImportAnimation = bImportAnimation;
	Settings.AnimationSampling = AnimationSampling;
	Settings.AnimationFramesPerSecond = FMath::Max(AnimationFramesPerSecond, 0.01f);
	Settings.bImportSun = bImportSun;
	Settings.bImportCameras = bImportCameras;
	Settings.LightIntensityScale = FMath::Max(0.0f, LightIntensityScale);
	return Settings;
}

#undef LOCTEXT_NAMESPACE
