#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Colors/SColorPicker.h"
#include "FDevNoteTag.h"

DECLARE_DELEGATE_OneParam(FOnTagAdded, FGuid);
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
        SLATE_EVENT(FOnTagAdded, OnTagAdded)
        SLATE_EVENT(FOnNewTagCreated, OnNewTagCreated)
        SLATE_EVENT(FOnOpened, OnTagListOpened)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    virtual ~SDevNoteTagPicker() override;
    
    void RefreshTagsList();
    void SetSelectedTagIDs(TArray<FGuid>* TagsArray)
    {
        SelectedTagIds = TagsArray;
    };

private:
    // UI Event Handlers
    FReply OnTagButtonClicked();
    TSharedRef<SWidget> GenerateTagDropdown();
    void OnMenuOpenChanged(bool bIsOpen);
    TSharedRef<ITableRow> GenerateTagRow(TSharedPtr<FDevNoteTag> InTag, const TSharedRef<STableViewBase>& OwnerTable);
    void OnTagClicked(TSharedPtr<FDevNoteTag> ClickedTag);
    void OnApplicationPreInputKeyDown(const FKeyEvent& InKeyEvent) const; 

    // Tag Creation
    FReply OnAddNewTagClicked();
    void OnNewTagNameChanged(const FText& NewText);
    void OnColorPickerClosed(FLinearColor NewColor);
    
    // Color Picker
    FReply OnColorButtonClicked();
    void OnColorPickerCommitted(FLinearColor NewColor);
    EVisibility GetColorPickerVisibility() const;
    TSharedRef<SWidget> CreateColourSwatch(const FLinearColor& Color);

    
    // Display Methods
    FText GetNewTagNameText() const;
    FLinearColor GetPendingTagColor() const;
    bool IsNewTagValid() const;
    
    // Helper methods
    TArray<TSharedPtr<FDevNoteTag>> GetUnselectedTags() const;

    FReply OnTagRowMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, TSharedPtr<FDevNoteTag> ClickedTag);
    void OnDeleteTagClicked(TSharedPtr<FDevNoteTag> TagToDelete);

private:
    // Data
    TArray<TSharedPtr<FDevNoteTag>>* TagsList = nullptr;
    TArray<FGuid>* SelectedTagIds = nullptr;
    TArray<TSharedPtr<FDevNoteTag>> UnselectedTags;
    FOnTagAdded OnTagAdded;
    FOnNewTagCreated OnNewTagCreated;
    FOnOpened OnTagListOpened;
    
    // UI Components
    TSharedPtr<SMenuAnchor> TagPickerAnchor;
    TSharedPtr<SListView<TSharedPtr<FDevNoteTag>>> TagListView;
    TSharedPtr<SEditableTextBox> NewTagNameBox;
    TSharedPtr<SColorBlock> ColorBlockWidget;
    TSharedPtr<SWidget> MenuContentWidget;
    TSharedPtr<SBox> ColorPickerContainer;
    TSharedPtr<SColorPicker> InlineColorPicker;
    
    // New Tag State
    FString NewTagName;
    FLinearColor PendingTagColor = FLinearColor::Blue;
    
    // Internal state tracking
    bool bColorPickerOpen = false;
    FDelegateHandle OutsideClickHandle;
};