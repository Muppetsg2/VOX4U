// Copyright (c) 2025-2026 Muppetsg2
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"
#include "VoxelEditing.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

class ITableRow;
class STableViewBase;
class UVoxel;
struct FAssetData;

/**
 * "Voxel Tools" tab of the voxel editor: the tool of the viewport's left click, the colours of the asset (add,
 * remove, choose the material of one) and the selected colour that new and painted voxels get.
 */
class SVoxelEditTools : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SVoxelEditTools)
		: _Voxel(nullptr)
		{}
		SLATE_ARGUMENT(UVoxel*, Voxel)
		SLATE_ARGUMENT(TSharedPtr<FVoxelEditState>, EditState)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Reads the colours of the asset again (after an edit, undo or reimport). */
	void Refresh();

private:

	TSharedRef<SWidget> MakeToolButton(EVoxelEditTool Tool, const FText& Label, const FText& Hint);
	ECheckBoxState IsToolChecked(EVoxelEditTool Tool) const;
	void OnToolChecked(ECheckBoxState State, EVoxelEditTool Tool);
	FText GetToolHint() const;

	TSharedRef<ITableRow> MakeColourRow(TSharedPtr<int32> Colour, const TSharedRef<STableViewBase>& Owner);
	void OnColourSelected(TSharedPtr<int32> Colour, ESelectInfo::Type SelectInfo);
	/** Selects the editor state's colour in the list. */
	void SyncSelection();

	FString GetSelectedMaterialPath() const;
	void OnSelectedMaterialChanged(const FAssetData& AssetData);
	bool HasSelectedColour() const;

	FReply OnAddColour();
	FReply OnRemoveColour();
	bool CanRemoveColour() const;

	EVisibility GetReimportWarningVisibility() const;

	TWeakObjectPtr<UVoxel> Voxel;
	TSharedPtr<FVoxelEditState> EditState;

	TArray<TSharedPtr<int32>> Colours;
	TArray<int32> VoxelCounts;
	TSharedPtr<SListView<TSharedPtr<int32>>> ColourList;
};
