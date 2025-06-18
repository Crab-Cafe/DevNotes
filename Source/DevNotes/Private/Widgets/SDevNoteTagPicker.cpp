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

void SDevNoteTagPicker::Construct(const FArguments& InArgs)
{
    TagsList = InArgs._AvailableTags;
    SelectedTagIds = InArgs._SelectedTagIds;
    OnSelectionChanged = InArgs._OnSelectionChanged;
    OnNewTagCreated = InArgs._OnNewTagCreated;
    OnTagListOpened = InArgs._OnTagListOpened;

    NewTagName.Empty();
    PendingTagColor = FLinearColor::Blue;

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
    if (TagPickerAnchor.IsValid() && TagPickerAnchor->IsOpen())
    {
        // Check if the new focused widget is within our menu content
        bool bIsWithinMenuContent = false;
        if (InNewFocusedWidget.IsValid() && MenuContentWidget.IsValid())
        {
            // Walk up the widget hierarchy to see if we're inside the menu content
            TSharedPtr<SWidget> CurrentWidget = InNewFocusedWidget;
            while (CurrentWidget.IsValid())
            {
                if (CurrentWidget == MenuContentWidget || CurrentWidget == TagPickerAnchor)
                {
                    bIsWithinMenuContent = true;
                    break;
                }
                CurrentWidget = CurrentWidget->GetParentWidget();
            }
        }

        // If focus moved outside our menu content, close it
        if (!bIsWithinMenuContent)
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

        // Register for focus change detection when menu opens
        if (FSlateApplication::IsInitialized() && !OutsideClickHandle.IsValid())
        {
            OutsideClickHandle = FSlateApplication::Get().OnFocusChanging().AddSP(
                SharedThis(this), &SDevNoteTagPicker::OnFocusChanging
            );
        }
    }
    else
    {
        // Unregister focus change detection when menu closes
        if (FSlateApplication::IsInitialized() && OutsideClickHandle.IsValid())
        {
            FSlateApplication::Get().OnFocusChanging().Remove(OutsideClickHandle);
            OutsideClickHandle.Reset();
        }
        
        // Clear the menu content reference
        MenuContentWidget.Reset();
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
                        .OnClicked_Lambda([this]() -> FReply {
                            // Create color picker window
                            TSharedRef<SWindow> ColorPickerWindow = SNew(SWindow)
                                .Title(FText::FromString(TEXT("Pick Tag Color")))
                                .ClientSize(FVector2D(300, 400))
                                .SupportsMaximize(false)
                                .SupportsMinimize(false)
                                .HasCloseButton(true)
                                .IsTopmostWindow(true);

                            ColorPickerWindow->SetContent(
                                SNew(SColorPicker)
                                .TargetColorAttribute(PendingTagColor)
                                .OnColorCommitted_Lambda([this, ColorPickerWindow](FLinearColor NewColor) {
                                    OnColorPickerClosed(NewColor);
                                    ColorPickerWindow->RequestDestroyWindow();
                                })
                            );

                            FSlateApplication::Get().AddWindowAsNativeChild(
                                ColorPickerWindow, 
                                FSlateApplication::Get().FindWidgetWindow(AsShared()).ToSharedRef()
                            );

                            return FReply::Handled();
                        })
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
    return SNew(STableRow<TSharedPtr<FDevNoteTag>>, OwnerTable)
    [
        SNew(STextBlock)
        .Text(FText::FromString(InTag->Name))
        .Margin(FMargin(8, 1))
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
    
    if (NewTagNameBox.IsValid())
    {
        NewTagNameBox->SetText(FText::GetEmpty());
    }

    return FReply::Handled();
}