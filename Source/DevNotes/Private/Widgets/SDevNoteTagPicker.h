#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Colors/SColorBlock.h"
#include "FDevNoteTag.h"

DECLARE_DELEGATE_OneParam(FOnTagSelectionChanged, const TArray<FGuid>&);
DECLARE_DELEGATE_OneParam(FOnNewTagCreated, const FDevNoteTag&);
DECLARE_DELEGATE(FOnOpened);


class DEVNOTES_API SDevNoteTagPicker : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SDevNoteTagPicker)
        : _AvailableTags(nullptr)
        , _SelectedTagIds(nullptr)
    {}
        SLATE_ARGUMENT(TArray<TSharedPtr<FDevNoteTag>>*, AvailableTags)
        SLATE_ARGUMENT(TArray<FGuid>*, SelectedTagIds)
        SLATE_EVENT(FOnTagSelectionChanged, OnSelectionChanged)
        SLATE_EVENT(FOnNewTagCreated, OnNewTagCreated)
        SLATE_EVENT(FOnOpened, OnTagListOpened)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual ~SDevNoteTagPicker() override;
    
    void RefreshTagsList();
    void UpdateSelectedTags();
    void SetSelectedTagIDs(TArray<FGuid>* TagsArray)
    {
        SelectedTagIds = TagsArray;
    };

private:
    // UI Event Handlers
    FReply OnTagButtonClicked();
    TSharedRef<SWidget> GenerateTagDropdown();
    void OnMenuOpenChanged(bool bIsOpen);
    void OnFocusChanging(const FFocusEvent& InFocusEvent, const FWeakWidgetPath& InOldFocusedWidgetPath, const TSharedPtr<SWidget>& InOldFocusedWidget, const FWidgetPath& InNewWidgetPath, const TSharedPtr<SWidget>& InNewFocusedWidget);  // Add this
    TSharedRef<ITableRow> GenerateTagRow(TSharedPtr<FDevNoteTag> InTag, const TSharedRef<STableViewBase>& OwnerTable);
    void OnTagSelectionChangedInternal(TSharedPtr<FDevNoteTag> SelectedItem, ESelectInfo::Type SelectInfo);
    
    // Tag Creation
    FReply OnAddNewTagClicked();
    void OnNewTagNameChanged(const FText& NewText);
    void OnColorPickerClosed(FLinearColor NewColor);
    
    // Display Methods
    FText GetSelectedTagsText() const;
    FText GetNewTagNameText() const;
    FLinearColor GetPendingTagColor() const;
    bool IsNewTagValid() const;
    
private:
    // Data
    TArray<TSharedPtr<FDevNoteTag>>* TagsList = nullptr;
    TArray<FGuid>* SelectedTagIds = nullptr;
    FOnTagSelectionChanged OnSelectionChanged;
    FOnNewTagCreated OnNewTagCreated;
    FOnOpened OnTagListOpened;
    
    // UI Components
    TSharedPtr<SMenuAnchor> TagPickerAnchor;
    TSharedPtr<SListView<TSharedPtr<FDevNoteTag>>> TagListView;
    TSharedPtr<SEditableTextBox> NewTagNameBox;
    TSharedPtr<SColorBlock> ColorBlockWidget;
    TSharedPtr<SWidget> MenuContentWidget;
    
    // New Tag State
    FString NewTagName;
    FLinearColor PendingTagColor = FLinearColor::Blue;
    
    // Internal state tracking
    bool bUpdatingSelection = false;
    FDelegateHandle OutsideClickHandle;
};