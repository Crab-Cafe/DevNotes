#include "SDevNoteTagPicker.h"

#include "DevNoteSubsystem.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Colors/SColorPicker.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Layout/Visibility.h"
#include "Widgets/Layout/SUniformGridPanel.h"

void SDevNoteTagPicker::Construct(const FArguments& InArgs)
{
    TagsList = InArgs._AvailableTags;
    SelectedTagIds = InArgs._SelectedTagIds;
    OnSelectionChanged = InArgs._OnSelectionChanged;
    OnNewTagCreated = InArgs._OnNewTagCreated;
    OnTagListOpened = InArgs._OnTagListOpened;

    NewTagName.Empty();
    PendingTagColor = FLinearColor::Blue;
    bColorPickerOpen = false;

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            SAssignNew(TagPickerAnchor, SMenuAnchor)
            .Placement(MenuPlacement_BelowAnchor)
            .OnGetMenuContent(this, &SDevNoteTagPicker::GenerateTagDropdown)
            .OnMenuOpenChanged(this, &SDevNoteTagPicker::OnMenuOpenChanged)
            [
                SNew(SButton)
                .Text(this, &SDevNoteTagPicker::GetSelectedTagsText)
                .OnClicked(this, &SDevNoteTagPicker::OnTagButtonClicked)
                .ToolTipText(FText::FromString(TEXT("Click to select tags")))
            ]
        ]
    ];
}


SDevNoteTagPicker::~SDevNoteTagPicker()
{
    // Unregister outside click notifications
    if (FSlateApplication::IsInitialized() && OutsideClickHandle.IsValid())
    {
        FSlateApplication::Get().OnFocusChanging().Remove(OutsideClickHandle);
    }
}

void SDevNoteTagPicker::OnFocusChanging(const FFocusEvent& InFocusEvent, const FWeakWidgetPath& InOldFocusedWidgetPath, const TSharedPtr<SWidget>& InOldFocusedWidget, const FWidgetPath& InNewWidgetPath, const TSharedPtr<SWidget>& InNewFocusedWidget)
{
    // Don't close if color picker is open
    if (bColorPickerOpen)
    {
        return;
    }

    if (TagPickerAnchor.IsValid() && TagPickerAnchor->IsOpen())
    {
        // Check if the new focused widget is within our menu or anchor
        bool bIsWithinMenu = false;
        
        // First check if we're clicking within the anchor itself
        if (InNewFocusedWidget == TagPickerAnchor)
        {
            bIsWithinMenu = true;
        }
        
        // Check if we're within the widget path that contains our anchor or menu content
        if (!bIsWithinMenu && InNewWidgetPath.IsValid())
        {
            for (int32 WidgetIndex = 0; WidgetIndex < InNewWidgetPath.Widgets.Num(); ++WidgetIndex)
            {
                TSharedPtr<SWidget> PathWidget = InNewWidgetPath.Widgets[WidgetIndex].Widget.ToSharedPtr();
                if (PathWidget.IsValid() && 
                    (PathWidget == TagPickerAnchor || 
                     PathWidget == MenuContentWidget ||
                     (InlineColorPicker.IsValid() && PathWidget == InlineColorPicker)))
                {
                    bIsWithinMenu = true;
                    break;
                }
            }
        }
        
        // Additional check: If we have a valid menu content widget, check the widget hierarchy
        if (!bIsWithinMenu && InNewFocusedWidget.IsValid() && MenuContentWidget.IsValid())
        {
            TSharedPtr<SWidget> CurrentWidget = InNewFocusedWidget;
            while (CurrentWidget.IsValid())
            {
                if (CurrentWidget == MenuContentWidget || 
                    CurrentWidget == TagPickerAnchor ||
                    (InlineColorPicker.IsValid() && CurrentWidget == InlineColorPicker))
                {
                    bIsWithinMenu = true;
                    break;
                }
                CurrentWidget = CurrentWidget->GetParentWidget();
            }
        }

        // If focus moved outside our menu system, close it
        if (!bIsWithinMenu)
        {
            TagPickerAnchor->SetIsOpen(false);
        }
    }
}



FReply SDevNoteTagPicker::OnTagButtonClicked()
{
    if (TagPickerAnchor.IsValid())
    {
        TagPickerAnchor->SetIsOpen(!TagPickerAnchor->IsOpen());
    }
    return FReply::Handled();
}


void SDevNoteTagPicker::OnMenuOpenChanged(bool bIsOpen)
{
    if (bIsOpen)
    {
        // Update selection when opening
        UpdateSelectedTags();
        
        // Notify parent that tag list was opened
        OnTagListOpened.ExecuteIfBound();

        // Register for outside click notifications instead of focus changes
        if (FSlateApplication::IsInitialized() && !OutsideClickHandle.IsValid() && MenuContentWidget.IsValid())
        {
            OutsideClickHandle = FSlateApplication::Get().GetPopupSupport().RegisterClickNotification(
                MenuContentWidget.ToSharedRef(),
                FOnClickedOutside::CreateSP(SharedThis(this), &SDevNoteTagPicker::OnClickedOutside)
            );
        }
    }
    else
    {
        // Unregister outside click detection when menu closes
        if (FSlateApplication::IsInitialized() && OutsideClickHandle.IsValid())
        {
            FSlateApplication::Get().GetPopupSupport().UnregisterClickNotification(OutsideClickHandle);
            OutsideClickHandle.Reset();
        }
        
        // Clear the menu content reference
        MenuContentWidget.Reset();
        bColorPickerOpen = false;
    }
}

void SDevNoteTagPicker::OnClickedOutside()
{
    // Don't close if color picker is open
    if (bColorPickerOpen)
    {
        return;
    }

    if (TagPickerAnchor.IsValid())
    {
        TagPickerAnchor->SetIsOpen(false);
    }
}


TSharedRef<SWidget> SDevNoteTagPicker::GenerateTagDropdown()
{
    // Store reference to the menu content for focus detection
    MenuContentWidget = SNew(SBorder)
        .Padding(8)
        .BorderImage(FAppStyle::GetBrush("Menu.Background"))
        [
            SNew(SBox)
            .MinDesiredWidth(250)
            [
                SNew(SVerticalBox)
                
                // Header
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Select Tags:")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                ]

                // Tag List with scroll
                + SVerticalBox::Slot().AutoHeight().MaxHeight(200)
                [
                    SNew(SScrollBox)
                    + SScrollBox::Slot()
                    [
                        SAssignNew(TagListView, SListView<TSharedPtr<FDevNoteTag>>)
                        .ListItemsSource(TagsList)
                        .OnGenerateRow(this, &SDevNoteTagPicker::GenerateTagRow)
                        .SelectionMode(ESelectionMode::Multi)
                        .OnSelectionChanged(this, &SDevNoteTagPicker::OnTagSelectionChangedInternal)
                        .ClearSelectionOnClick(false)
                    ]
                ]

                // Separator
                + SVerticalBox::Slot().AutoHeight().Padding(0, 8)
                [
                    SNew(SBorder)
                    .BorderImage(FAppStyle::GetBrush("Menu.Separator"))
                    .Padding(0, 1)
                ]

                // New Tag Section Header
                + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("Create New Tag:")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
                ]

                // New Tag Input Row
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SHorizontalBox)
                    
                    // Name input
                    + SHorizontalBox::Slot().FillWidth(1.0f)
                    [
                        SAssignNew(NewTagNameBox, SEditableTextBox)
                        .HintText(FText::FromString(TEXT("Tag name")))
                        .Text(this, &SDevNoteTagPicker::GetNewTagNameText)
                        .OnTextChanged(this, &SDevNoteTagPicker::OnNewTagNameChanged)
                    ]
                    
                    // Color picker button
                    + SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
                    [
                        SNew(SButton)
                        .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                        .ToolTipText(FText::FromString(TEXT("Pick color")))
                        .OnClicked(this, &SDevNoteTagPicker::OnColorButtonClicked)
                        [
                            SAssignNew(ColorBlockWidget, SColorBlock)
                            .Color(this, &SDevNoteTagPicker::GetPendingTagColor)
                            .Size(FVector2D(20, 20))
                            .AlphaDisplayMode(EColorBlockAlphaDisplayMode::Ignore)
                        ]
                    ]

                    // Add button
                    + SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
                    [
                        SNew(SButton)
                        .Text(FText::FromString(TEXT("Add")))
                        .OnClicked(this, &SDevNoteTagPicker::OnAddNewTagClicked)
                        .IsEnabled(this, &SDevNoteTagPicker::IsNewTagValid)
                    ]
                ]

                // Simple Color Palette (shown when color button is clicked)
                + SVerticalBox::Slot().AutoHeight().Padding(0, 4)
                [
                    SAssignNew(ColorPickerContainer, SBox)
                    .Visibility(this, &SDevNoteTagPicker::GetColorPickerVisibility)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(STextBlock)
                            .Text(FText::FromString(TEXT("Choose Colour:")))
                            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 8))
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 4)
                        [
                            SNew(SUniformGridPanel)
                            .SlotPadding(FMargin(2))
                            
                            // Row 1
                            + SUniformGridPanel::Slot(0, 0)
                            [
                                SNew(SButton)
                                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                                .OnClicked_Lambda([this]() -> FReply {
                                    PendingTagColor = FLinearColor::Red;
                                    bColorPickerOpen = false;
                                    return FReply::Handled();
                                })
                                [
                                    SNew(SColorBlock)
                                    .Color(FLinearColor::Red)
                                    .Size(FVector2D(16, 16))
                                ]
                            ]
                            + SUniformGridPanel::Slot(1, 0)
                            [
                                SNew(SButton)
                                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                                .OnClicked_Lambda([this]() -> FReply {
                                    PendingTagColor = FLinearColor::Green;
                                    bColorPickerOpen = false;
                                    return FReply::Handled();
                                })
                                [
                                    SNew(SColorBlock)
                                    .Color(FLinearColor::Green)
                                    .Size(FVector2D(16, 16))
                                ]
                            ]
                            + SUniformGridPanel::Slot(2, 0)
                            [
                                SNew(SButton)
                                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                                .OnClicked_Lambda([this]() -> FReply {
                                    PendingTagColor = FLinearColor::Blue;
                                    bColorPickerOpen = false;
                                    return FReply::Handled();
                                })
                                [
                                    SNew(SColorBlock)
                                    .Color(FLinearColor::Blue)
                                    .Size(FVector2D(16, 16))
                                ]
                            ]
                            + SUniformGridPanel::Slot(3, 0)
                            [
                                SNew(SButton)
                                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                                .OnClicked_Lambda([this]() -> FReply {
                                    PendingTagColor = FLinearColor::Yellow;
                                    bColorPickerOpen = false;
                                    return FReply::Handled();
                                })
                                [
                                    SNew(SColorBlock)
                                    .Color(FLinearColor::Yellow)
                                    .Size(FVector2D(16, 16))
                                ]
                            ]
                            
                            // Row 2
                            + SUniformGridPanel::Slot(0, 1)
                            [
                                SNew(SButton)
                                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                                .OnClicked_Lambda([this]() -> FReply {
                                    PendingTagColor = FLinearColor(1.0f, 0.5f, 0.0f); // Orange
                                    bColorPickerOpen = false;
                                    return FReply::Handled();
                                })
                                [
                                    SNew(SColorBlock)
                                    .Color(FLinearColor(1.0f, 0.5f, 0.0f))
                                    .Size(FVector2D(16, 16))
                                ]
                            ]
                            + SUniformGridPanel::Slot(1, 1)
                            [
                                SNew(SButton)
                                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                                .OnClicked_Lambda([this]() -> FReply {
                                    PendingTagColor = FLinearColor(0.5f, 0.0f, 0.5f); // Purple
                                    bColorPickerOpen = false;
                                    return FReply::Handled();
                                })
                                [
                                    SNew(SColorBlock)
                                    .Color(FLinearColor(0.5f, 0.0f, 0.5f))
                                    .Size(FVector2D(16, 16))
                                ]
                            ]
                            + SUniformGridPanel::Slot(2, 1)
                            [
                                SNew(SButton)
                                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                                .OnClicked_Lambda([this]() -> FReply {
                                    PendingTagColor = FLinearColor(0.0f, 0.5f, 0.5f); // Cyan
                                    bColorPickerOpen = false;
                                    return FReply::Handled();
                                })
                                [
                                    SNew(SColorBlock)
                                    .Color(FLinearColor(0.0f, 0.5f, 0.5f))
                                    .Size(FVector2D(16, 16))
                                ]
                            ]
                            + SUniformGridPanel::Slot(3, 1)
                            [
                                SNew(SButton)
                                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                                .OnClicked_Lambda([this]() -> FReply {
                                    PendingTagColor = FLinearColor(0.5f, 0.5f, 0.5f); // Gray
                                    bColorPickerOpen = false;
                                    return FReply::Handled();
                                })
                                [
                                    SNew(SColorBlock)
                                    .Color(FLinearColor(0.5f, 0.5f, 0.5f))
                                    .Size(FVector2D(16, 16))
                                ]
                            ]
                        ]
                    ]
                ]

                // Close button at bottom
                + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
                [
                    SNew(SButton)
                    .Text(FText::FromString(TEXT("Close")))
                    .HAlign(HAlign_Center)
                    .OnClicked_Lambda([this]() -> FReply {
                        if (TagPickerAnchor.IsValid())
                        {
                            TagPickerAnchor->SetIsOpen(false);
                        }
                        return FReply::Handled();
                    })
                ]
            ]
        ];

    return MenuContentWidget.ToSharedRef();
}

FReply SDevNoteTagPicker::OnColorButtonClicked()
{
    bColorPickerOpen = !bColorPickerOpen;
    return FReply::Handled();
}

void SDevNoteTagPicker::OnColorPickerCommitted(FLinearColor NewColor)
{
    PendingTagColor = NewColor;
    bColorPickerOpen = false;
}

EVisibility SDevNoteTagPicker::GetColorPickerVisibility() const
{
    return bColorPickerOpen ? EVisibility::Visible : EVisibility::Collapsed;
}

void SDevNoteTagPicker::RefreshTagsList()
{
    if (TagListView.IsValid())
    {
        TagListView->RequestListRefresh();
        UpdateSelectedTags();
    }
}

void SDevNoteTagPicker::UpdateSelectedTags()
{
    if (!TagListView.IsValid() || !SelectedTagIds || bUpdatingSelection)
        return;
        
    bUpdatingSelection = true;
    
    TArray<TSharedPtr<FDevNoteTag>> SelectedItems;
    if (TagsList)
    {
        for (const auto& NoteTag : *TagsList)
        {
            if (SelectedTagIds->Contains(NoteTag->Id))
            {
                SelectedItems.Add(NoteTag);
            }
        }
    }
    
    TagListView->ClearSelection();
    TagListView->SetItemSelection(SelectedItems, true);
    bUpdatingSelection = false;
}

TSharedRef<ITableRow> SDevNoteTagPicker::GenerateTagRow(TSharedPtr<FDevNoteTag> InTag, const TSharedRef<STableViewBase>& OwnerTable)
{
    // Convert packed ARGB to FLinearColor
    FColor TagColor;
    TagColor.DWColor() = InTag->Colour;
    FLinearColor DisplayColor = FLinearColor::FromSRGBColor(TagColor);

    return SNew(STableRow<TSharedPtr<FDevNoteTag>>, OwnerTable)
    [
        SNew(SHorizontalBox)
        
        // Color swatch
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(FMargin(8, 1, 6, 1))
        [
            SNew(SColorBlock)
            .Color(DisplayColor)
            .Size(FVector2D(12, 12))
            .AlphaDisplayMode(EColorBlockAlphaDisplayMode::Ignore)
            .CornerRadius(FVector4(2, 2, 2, 2))
        ]
        
        // Tag name
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .VAlign(VAlign_Center)
        .Padding(FMargin(0, 1, 8, 1))
        [
            SNew(STextBlock)
            .Text(FText::FromString(InTag->Name))
        ]
    ];
}

void SDevNoteTagPicker::OnTagSelectionChangedInternal(TSharedPtr<FDevNoteTag> SelectedItem, ESelectInfo::Type SelectInfo)
{
    if (bUpdatingSelection || !SelectedItem.IsValid() || !SelectedTagIds)
        return;

    FGuid TagId = SelectedItem->Id;
    
    if (SelectInfo == ESelectInfo::OnMouseClick || SelectInfo == ESelectInfo::OnKeyPress)
    {
        // Get current ListView selection to sync with our data
        TArray<TSharedPtr<FDevNoteTag>> CurrentSelection = TagListView->GetSelectedItems();
        
        // Clear and rebuild our SelectedTagIds based on ListView selection
        SelectedTagIds->Empty();
        for (const auto& Item : CurrentSelection)
        {
            if (Item.IsValid())
            {
                SelectedTagIds->Add(Item->Id);
            }
        }
        
        // Notify parent of change
        OnSelectionChanged.ExecuteIfBound(*SelectedTagIds);
    }
}

FText SDevNoteTagPicker::GetSelectedTagsText() const
{
    if (!SelectedTagIds || !TagsList || SelectedTagIds->Num() == 0)
    {
        return FText::FromString(TEXT("No tags selected"));
    }

    // Build a string with all selected tag names
    FString Result;
    for (const auto& TagPtr : *TagsList)
    {
        if (TagPtr.IsValid() && SelectedTagIds->Contains(TagPtr->Id))
        {
            if (!Result.IsEmpty()) 
            {
                Result += TEXT(", ");
            }
            Result += TagPtr->Name;
        }
    }

    // If we somehow didn't find any matching tags, show fallback
    if (Result.IsEmpty())
    {
        return FText::FromString(TEXT("No tags selected"));
    }

    return FText::FromString(Result);
}


FText SDevNoteTagPicker::GetNewTagNameText() const 
{ 
    return FText::FromString(NewTagName); 
}

void SDevNoteTagPicker::OnNewTagNameChanged(const FText& NewText) 
{ 
    NewTagName = NewText.ToString(); 
}

void SDevNoteTagPicker::OnColorPickerClosed(FLinearColor NewColor)
{
    PendingTagColor = NewColor;
}

FLinearColor SDevNoteTagPicker::GetPendingTagColor() const 
{ 
    return PendingTagColor; 
}

bool SDevNoteTagPicker::IsNewTagValid() const 
{ 
    return !NewTagName.IsEmpty() && NewTagName.Len() > 0; 
}

FReply SDevNoteTagPicker::OnAddNewTagClicked()
{
    if (!IsNewTagValid())
        return FReply::Handled();

    // Create new tag
    FDevNoteTag NewTag;
    NewTag.Id = FGuid::NewGuid();
    NewTag.Name = NewTagName;
    
    // Convert color to int format expected by the system
    FColor SRGBColor = PendingTagColor.ToFColor(true);
    NewTag.Colour = SRGBColor.ToPackedARGB();

    // Add to tags list
    if (TagsList)
    {
        TagsList->Add(MakeShared<FDevNoteTag>(NewTag));
        RefreshTagsList();
    }

    // Notify parent that new tag was created
    OnNewTagCreated.ExecuteIfBound(NewTag);
    
    // Reset form
    NewTagName.Empty();
    PendingTagColor = FLinearColor::Blue;
    bColorPickerOpen = false;
    
    if (NewTagNameBox.IsValid())
    {
        NewTagNameBox->SetText(FText::GetEmpty());
    }

    return FReply::Handled();
}