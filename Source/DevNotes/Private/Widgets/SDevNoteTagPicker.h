#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "FDevNoteTag.h"
#include "Widgets/Colors/SColorBlock.h"

// Delegate: Called when selection changes (returns list of selected tag IDs)
DECLARE_DELEGATE_OneParam(FOnTagSelectionChanged, const TArray<FGuid>&)
// Delegate: Called when a new tag is created
DECLARE_DELEGATE_OneParam(FOnNewTagCreated, const FDevNoteTag&)

class SDevNoteTagPicker : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SDevNoteTagPicker) {}
    SLATE_ARGUMENT(TArray<TSharedPtr<FDevNoteTag>>*, AvailableTags)
    SLATE_ARGUMENT(TArray<FGuid>*, SelectedTagIds)
    SLATE_EVENT(FOnTagSelectionChanged, OnSelectionChanged)
    SLATE_EVENT(FOnNewTagCreated, OnNewTagCreated)
SLATE_END_ARGS()

void Construct(const FArguments& InArgs);

private:
    // References provided by parent
    TArray<TSharedPtr<FDevNoteTag>>* TagsList = nullptr;
    TArray<FGuid>* SelectedTagIds = nullptr;

    FOnTagSelectionChanged OnSelectionChanged;
    FOnNewTagCreated OnNewTagCreated;

    TSharedPtr<SMenuAnchor> TagPickerAnchor;
    TSharedPtr<SListView<TSharedPtr<FDevNoteTag>>> TagListView;
    TSharedPtr<SEditableTextBox> NewTagNameBox;
    FString NewTagName;
    int32 NewTagColor = 0;

    FLinearColor PendingTagColor = FLinearColor::White; // or any default
    TSharedPtr<SColorBlock> ColorBlockWidget;

    FLinearColor GetPendingTagColor() const { return PendingTagColor; }

    FReply OnTagButtonClicked();
    TSharedRef<SWidget> GenerateTagDropdown();

    // Tag list
    TSharedRef<ITableRow> GenerateTagRow(TSharedPtr<FDevNoteTag>, const TSharedRef<STableViewBase>&);
    void OnTagSelectionChangedInternal(TSharedPtr<FDevNoteTag>, ESelectInfo::Type);
    FText GetSelectedTagsText() const;

    // New tag controls
    FText GetNewTagNameText() const;
    void OnNewTagNameChanged(const FText&);
    FText GetNewTagColorText() const;
    void OnNewTagColorChanged(const FText&);
    bool IsNewTagValid() const;
    FReply OnAddNewTagClicked();
};