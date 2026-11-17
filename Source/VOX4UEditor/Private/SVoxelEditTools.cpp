// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#include "SVoxelEditTools.h"
#include "Engine/StaticMesh.h"
#include "EditorFramework/AssetImportData.h"
#include "Materials/MaterialInterface.h"
#include "PropertyCustomizationHelpers.h"
#include "Styling/CoreStyle.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Voxel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "VoxelEditTools"

namespace VoxelEditToolsPrivate
{
	FSlateFontInfo HeaderFont()
	{
		return FCoreStyle::GetDefaultFontStyle("Bold", 10);
	}

	/** Material a colour is drawn with: its own, or the one of its mesh. */
	UMaterialInterface* DrawnMaterial(const UVoxel& Voxel, int32 Colour)
	{
		if (UMaterialInterface* Material = Voxel.GetMaterial(Colour))
		{
			return Material;
		}
		const UStaticMesh* Cube = Voxel.GetColourMesh(Colour);
		return Cube ? Cube->GetMaterial(0) : nullptr;
	}

	/** Name shown for a colour: the material it is drawn with. */
	FText ColourName(const UVoxel& Voxel, int32 Colour)
	{
		if (const UMaterialInterface* Material = DrawnMaterial(Voxel, Colour))
		{
			return FText::FromString(Material->GetName());
		}
		return Voxel.GetColourMesh(Colour) ? LOCTEXT("MeshMaterial", "(material of the mesh)") : LOCTEXT("NoMesh", "(no mesh)");
	}
}

void SVoxelEditTools::Construct(const FArguments& InArgs)
{
	using namespace VoxelEditToolsPrivate;

	Voxel = InArgs._Voxel;
	EditState = InArgs._EditState.IsValid() ? InArgs._EditState : MakeShared<FVoxelEditState>();
	EditState->OnChanged.AddSP(this, &SVoxelEditTools::SyncSelection);

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 8.0f, 8.0f, 4.0f)
		[
			SNew(STextBlock)
			.Font(HeaderFont())
			.Text(LOCTEXT("ToolHeader", "Tool"))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 12.0f, 4.0f)
			[
				MakeToolButton(EVoxelEditTool::Select, LOCTEXT("ToolSelect", "Camera"), LOCTEXT("ToolSelectTip", "Clicks in the viewport only move the camera."))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 12.0f, 4.0f)
			[
				MakeToolButton(EVoxelEditTool::Add, LOCTEXT("ToolAdd", "Add"), LOCTEXT("ToolAddTip", "Click: add a voxel of the selected colour on the face under the cursor, or on the floor. Shift+click: remove."))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 12.0f, 4.0f)
			[
				MakeToolButton(EVoxelEditTool::Remove, LOCTEXT("ToolRemove", "Remove"), LOCTEXT("ToolRemoveTip", "Click: remove the voxel under the cursor."))
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 12.0f, 4.0f)
			[
				MakeToolButton(EVoxelEditTool::Paint, LOCTEXT("ToolPaint", "Paint"), LOCTEXT("ToolPaintTip", "Click: give the voxel under the cursor the selected colour. Shift+click: select the colour of the voxel."))
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 2.0f, 8.0f, 8.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.Text(this, &SVoxelEditTools::GetToolHint)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 4.0f)
		[
			SNew(STextBlock)
			.Font(HeaderFont())
			.Text(LOCTEXT("ColoursHeader", "Colours"))
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(8.0f, 0.0f)
		[
			SAssignNew(ColourList, SListView<TSharedPtr<int32>>)
			.ListItemsSource(&Colours)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow(this, &SVoxelEditTools::MakeColourRow)
			.OnSelectionChanged(this, &SVoxelEditTools::OnColourSelected)
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 6.0f, 8.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MaterialLabel", "Material"))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SObjectPropertyEntryBox)
				.IsEnabled(this, &SVoxelEditTools::HasSelectedColour)
				.AllowedClass(UMaterialInterface::StaticClass())
				.ObjectPath(this, &SVoxelEditTools::GetSelectedMaterialPath)
				.OnObjectChanged(this, &SVoxelEditTools::OnSelectedMaterialChanged)
				.AllowClear(true)
				.ThumbnailPool(UThumbnailManager::Get().GetSharedThumbnailPool())
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 4.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("AddColour", "Add Colour"))
				.ToolTipText(LOCTEXT("AddColourTip", "Add a colour with the material of the selected one; then choose its material."))
				.OnClicked(this, &SVoxelEditTools::OnAddColour)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("RemoveColour", "Remove Colour"))
				.ToolTipText(LOCTEXT("RemoveColourTip", "Remove the selected colour and its voxels. The last colour cannot be removed."))
				.IsEnabled(this, &SVoxelEditTools::CanRemoveColour)
				.OnClicked(this, &SVoxelEditTools::OnRemoveColour)
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 6.0f, 8.0f, 8.0f)
		[
			SNew(STextBlock)
			.AutoWrapText(true)
			.ColorAndOpacity(FLinearColor(1.0f, 0.75f, 0.2f))
			.Visibility(this, &SVoxelEditTools::GetReimportWarningVisibility)
			.Text(LOCTEXT("ReimportWarning", "This asset was imported: reimporting the .vox file replaces these edits."))
		]
	];

	Refresh();
}

void SVoxelEditTools::Refresh()
{
	const UVoxel* Asset = Voxel.Get();
	const int32 NumColours = Asset ? Asset->GetNumColours() : 0;

	Colours.Reset(NumColours);
	VoxelCounts.Init(0, NumColours);
	for (int32 Colour = 0; Colour < NumColours; ++Colour)
	{
		Colours.Add(MakeShared<int32>(Colour));
	}
	if (Asset)
	{
		for (const TPair<FIntVector, uint8>& Cell : Asset->Voxels)
		{
			if (VoxelCounts.IsValidIndex(Cell.Value))
			{
				++VoxelCounts[Cell.Value];
			}
		}
	}

	EditState->Colour = FMath::Clamp(EditState->Colour, 0, FMath::Max(NumColours - 1, 0));
	if (ColourList.IsValid())
	{
		ColourList->RequestListRefresh();
	}
	SyncSelection();
}

TSharedRef<SWidget> SVoxelEditTools::MakeToolButton(EVoxelEditTool Tool, const FText& Label, const FText& Hint)
{
	return SNew(SCheckBox)
		.Style(&FCoreStyle::Get().GetWidgetStyle<FCheckBoxStyle>("RadioButton"))
		.ToolTipText(Hint)
		.IsChecked(this, &SVoxelEditTools::IsToolChecked, Tool)
		.OnCheckStateChanged(this, &SVoxelEditTools::OnToolChecked, Tool)
		[
			SNew(STextBlock)
			.Text(Label)
		];
}

ECheckBoxState SVoxelEditTools::IsToolChecked(EVoxelEditTool Tool) const
{
	return EditState->Tool == Tool ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SVoxelEditTools::OnToolChecked(ECheckBoxState State, EVoxelEditTool Tool)
{
	if (State == ECheckBoxState::Checked)
	{
		EditState->Tool = Tool;
		EditState->OnChanged.Broadcast();
	}
}

FText SVoxelEditTools::GetToolHint() const
{
	switch (EditState->Tool)
	{
		case EVoxelEditTool::Add:
			return LOCTEXT("HintAdd", "Click: add a voxel on the outlined place. Shift+click: remove the voxel under the cursor. Ctrl+Z undoes.");
		case EVoxelEditTool::Remove:
			return LOCTEXT("HintRemove", "Click: remove the outlined voxel. Ctrl+Z undoes.");
		case EVoxelEditTool::Paint:
			return LOCTEXT("HintPaint", "Click: paint the outlined voxel with the selected colour. Shift+click: select its colour.");
		case EVoxelEditTool::Select:
		default:
			return LOCTEXT("HintSelect", "Choose a tool to edit the voxels with the left mouse button. The camera works as in the Static Mesh Editor.");
	}
}

TSharedRef<ITableRow> SVoxelEditTools::MakeColourRow(TSharedPtr<int32> Colour, const TSharedRef<STableViewBase>& Owner)
{
	const int32 Index = Colour.IsValid() ? *Colour : INDEX_NONE;
	const UVoxel* Asset = Voxel.Get();
	const FText Name = Asset ? VoxelEditToolsPrivate::ColourName(*Asset, Index) : FText::GetEmpty();
	const int32 Count = VoxelCounts.IsValidIndex(Index) ? VoxelCounts[Index] : 0;

	return SNew(STableRow<TSharedPtr<int32>>, Owner)
		.Padding(FMargin(4.0f, 2.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 8.0f, 0.0f)
			[
				SNew(SBox)
				.MinDesiredWidth(24.0f)
				[
					SNew(STextBlock)
					.Text(FText::AsNumber(Index))
				]
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text(Name)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.ToolTipText(LOCTEXT("VoxelCountTip", "Voxels of this colour"))
				.Text(FText::AsNumber(Count))
			]
		];
}

void SVoxelEditTools::OnColourSelected(TSharedPtr<int32> Colour, ESelectInfo::Type SelectInfo)
{
	if (Colour.IsValid() && SelectInfo != ESelectInfo::Direct && EditState->Colour != *Colour)
	{
		EditState->Colour = *Colour;
		EditState->OnChanged.Broadcast();
	}
}

void SVoxelEditTools::SyncSelection()
{
	if (ColourList.IsValid() && Colours.IsValidIndex(EditState->Colour))
	{
		const TSharedPtr<int32>& Item = Colours[EditState->Colour];
		if (!ColourList->IsItemSelected(Item))
		{
			ColourList->SetSelection(Item, ESelectInfo::Direct);
		}
		ColourList->RequestScrollIntoView(Item);
	}
}

bool SVoxelEditTools::HasSelectedColour() const
{
	const UVoxel* Asset = Voxel.Get();
	return Asset && EditState->Colour >= 0 && EditState->Colour < Asset->GetNumColours();
}

FString SVoxelEditTools::GetSelectedMaterialPath() const
{
	const UVoxel* Asset = Voxel.Get();
	const UMaterialInterface* Material = Asset ? Asset->GetMaterial(EditState->Colour) : nullptr;
	return Material ? Material->GetPathName() : FString();
}

void SVoxelEditTools::OnSelectedMaterialChanged(const FAssetData& AssetData)
{
	if (UVoxel* Asset = Voxel.Get())
	{
		UMaterialInterface* Material = Cast<UMaterialInterface>(AssetData.GetAsset());
		const int32 Colour = EditState->Colour;
		VoxelEditing::ApplyEdit(*Asset, LOCTEXT("SetColourMaterial", "Set Colour Material"), [Colour, Material](UVoxel& Edited) { return VoxelEditing::SetColourMaterial(Edited, Colour, Material); });
	}
}

FReply SVoxelEditTools::OnAddColour()
{
	UVoxel* Asset = Voxel.Get();
	if (!Asset)
	{
		return FReply::Handled();
	}

	// A copy of the selected colour (the material it is drawn with), to be given its own material.
	UMaterialInterface* Material = VoxelEditToolsPrivate::DrawnMaterial(*Asset, EditState->Colour);

	int32 Added = INDEX_NONE;
	VoxelEditing::ApplyEdit(*Asset, LOCTEXT("AddColourTransaction", "Add Colour"), [Material, &Added](UVoxel& Edited)
	{
		Added = VoxelEditing::AddColour(Edited, Material);
		return Added != INDEX_NONE;
	});
	if (Added != INDEX_NONE)
	{
		EditState->Colour = Added;
		Refresh();
		EditState->OnChanged.Broadcast();
	}
	return FReply::Handled();
}

FReply SVoxelEditTools::OnRemoveColour()
{
	if (UVoxel* Asset = Voxel.Get())
	{
		const int32 Colour = EditState->Colour;
		VoxelEditing::ApplyEdit(*Asset, LOCTEXT("RemoveColourTransaction", "Remove Colour"), [Colour](UVoxel& Edited) { return VoxelEditing::RemoveColour(Edited, Colour); });
		Refresh();
	}
	return FReply::Handled();
}

bool SVoxelEditTools::CanRemoveColour() const
{
	const UVoxel* Asset = Voxel.Get();
	return Asset && Asset->GetNumColours() > 1 && EditState->Colour >= 0 && EditState->Colour < Asset->GetNumColours();
}

EVisibility SVoxelEditTools::GetReimportWarningVisibility() const
{
	const UVoxel* Asset = Voxel.Get();
	const UAssetImportData* ImportData = Asset ? Asset->GetAssetImportData() : nullptr;
	return ImportData && !ImportData->GetFirstFilename().IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
