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
#include "Input/Events.h"

void SDevNoteTagPicker::Construct(const FArguments& InArgs)
{
    TagsList = InArgs._AvailableTags;
    SelectedTagIds = InArgs._SelectedTagIds;
    OnTagAdded = InArgs._OnTagAdded;
    OnNewTagCreated = InArgs._OnNewTagCreated;
    OnTagListOpened = InArgs._OnTagListOpened;

    NewTagName.Empty();
    PendingTagColor = FLinearColor::Blue;
    bColorPickerOpen = false;

    ChildSlot
    [
        SAssignNew(TagPickerAnchor, SMenuAnchor)
        .OnMenuOpenChanged(this, &SDevNoteTagPicker::OnMenuOpenChanged)
        .Placement(MenuPlacement_BelowAnchor)
        .Method(EPopupMethod::UseCurrentWindow)
        [
            SNew(SButton)
            .ButtonStyle(FAppStyle::Get(), "SimpleButton")
            .ToolTipText(FText::FromString(TEXT("Add Tag")))
            .OnClicked(this, &SDevNoteTagPicker::OnTagButtonClicked)
            .ContentPadding(FMargin(2, 1))
            [
                SNew(SBox)
                .WidthOverride(16)
                .HeightOverride(16)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(TEXT("+")))
                    .Justification(ETextJustify::Center)
                    .Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
                    .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                ]
            ]
        ]
        .MenuContent(
            SNew(SBox)
            [
                this->GenerateTagDropdown()
            ])
    ];
}

SDevNoteTagPicker::~SDevNoteTagPicker()
{
    // Unregister outside click notifications
    if (FSlateApplication::IsInitialized() && OutsideClickHandle.IsValid())
    {
        FSlateApplication::Get().OnApplicationPreInputKeyDownListener().Remove(OutsideClickHandle);
    }
}

void SDevNoteTagPicker::OnApplicationPreInputKeyDown(const FKeyEvent& InKeyEvent) const
{
    // Only care about left mouse button clicks
    if (InKeyEvent.GetKey() != EKeys::LeftMouseButton)
    {
        return;
    }

    // Don't close if color picker is open
    if (bColorPickerOpen)
    {
        return;
    }

    if (!TagPickerAnchor.IsValid() || !TagPickerAnchor->IsOpen())
    {
        return;
    }

    // Get the widget under the mouse cursor
    FVector2D MousePosition = FSlateApplication::Get().GetCursorPos();
    FWidgetPath WidgetPath = FSlateApplication::Get().LocateWindowUnderMouse(MousePosition, FSlateApplication::Get().GetInteractiveTopLevelWindows());
    
    if (WidgetPath.IsValid())
    {
        // Check if any widget in the path is part of our menu system
        for (int32 WidgetIndex = 0; WidgetIndex < WidgetPath.Widgets.Num(); ++WidgetIndex)
        {
            TSharedPtr<SWidget> Widget = WidgetPath.Widgets[WidgetIndex].Widget.ToSharedPtr();
            if (Widget.IsValid())
            {
                // Check if this widget is our anchor or menu content
                if (Widget == TagPickerAnchor || Widget == MenuContentWidget)
                {
                    return; // Don't close, we're clicking within our menu
                }
                
                // Check if this widget is a child of our menu content
                if (MenuContentWidget.IsValid())
                {
                    TSharedPtr<SWidget> CurrentParent = Widget->GetParentWidget();
                    while (CurrentParent.IsValid())
                    {
                        if (CurrentParent == MenuContentWidget)
                        {
                            return; // Don't close, we're clicking within our menu
                        }
                        CurrentParent = CurrentParent->GetParentWidget();
                    }
                }
            }
        }
    }

    // If we get here, we're clicking outside the menu - close it
    TagPickerAnchor->SetIsOpen(false);
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
        // Update the list of unselected tags
        UnselectedTags = GetUnselectedTags();
        
        // Refresh the list view
        if (TagListView.IsValid())
        {
            TagListView->RequestListRefresh();
        }
        
        // Notify parent that tag list was opened
        OnTagListOpened.ExecuteIfBound();

        // Register for mouse button down events to detect outside clicks
        if (FSlateApplication::IsInitialized())
        {
            TSharedRef<SDevNoteTagPicker> shared = SharedThis(this);
            OutsideClickHandle = FSlateApplication::Get().OnApplicationPreInputKeyDownListener().AddSP(
                shared, &SDevNoteTagPicker::OnApplicationPreInputKeyDown
            );
        }
    }
    else
    {
        // Unregister mouse button down events when menu closes
        if (FSlateApplication::IsInitialized() && OutsideClickHandle.IsValid())
        {
            FSlateApplication::Get().OnApplicationPreInputKeyDownListener().Remove(OutsideClickHandle);
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

TArray<TSharedPtr<FDevNoteTag>> SDevNoteTagPicker::GetUnselectedTags() const
{
    TArray<TSharedPtr<FDevNoteTag>> Result;
    
    if (!TagsList || !SelectedTagIds)
    {
        return Result;
    }
    
    for (const auto& NoteTag : *TagsList)
    {
        if (NoteTag.IsValid() && !SelectedTagIds->Contains(NoteTag->Id))
        {
            Result.Add(NoteTag);
        }
    }
    
    return Result;
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
                    .Text(FText::FromString(TEXT("Add Tags:")))
                    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
                ]

                // Tag List with scroll
                + SVerticalBox::Slot().AutoHeight().MaxHeight(200)
                [
                    SNew(SScrollBox)
                    + SScrollBox::Slot()
                    [
                        SAssignNew(TagListView, SListView<TSharedPtr<FDevNoteTag>>)
                        .ListItemsSource(&UnselectedTags)
                        .OnGenerateRow(this, &SDevNoteTagPicker::GenerateTagRow)
                        .SelectionMode(ESelectionMode::None)
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
TSharedRef<ITableRow> SDevNoteTagPicker::GenerateTagRow(TSharedPtr<FDevNoteTag> InTag, const TSharedRef<STableViewBase>& OwnerTable)
{
    // Convert packed ARGB to FLinearColor
    FColor TagColor;
    TagColor.DWColor() = InTag->Colour;
    FLinearColor DisplayColor = FLinearColor::FromSRGBColor(TagColor);

    return SNew(STableRow<TSharedPtr<FDevNoteTag>>, OwnerTable)
        [
            SNew(SHorizontalBox)
            
            // Main button for tag selection
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                SNew(SButton)
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .OnClicked_Lambda([this, InTag]() -> FReply {
                    OnTagClicked(InTag);
                    return FReply::Handled();
                })
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
                ]
            ]
            
            // Delete button (X)
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            .Padding(FMargin(2, 1, 4, 1))
            [
                SNew(SButton)
                .ButtonStyle(FAppStyle::Get(), "SimpleButton")
                .ContentPadding(FMargin(2, 0))
                .ToolTipText(FText::FromString(FString::Printf(TEXT("Delete tag '%s'"), *InTag->Name)))
                .OnClicked_Lambda([this, InTag]() -> FReply {
                    OnDeleteTagClicked(InTag);
                    return FReply::Handled();
                })
                [
                    SNew(STextBlock)
                    .Text(FText::FromString("x"))
                    .Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
                    .ColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f)))
                ]
            ]
        ];

}


void SDevNoteTagPicker::OnTagClicked(TSharedPtr<FDevNoteTag> ClickedTag)
{
    if (ClickedTag.IsValid())
    {
        // Notify parent that a tag should be added
        OnTagAdded.ExecuteIfBound(ClickedTag->Id);
        
        // Close the dropdown
        if (TagPickerAnchor.IsValid())
        {
            TagPickerAnchor->SetIsOpen(false);
        }
    }
}

void SDevNoteTagPicker::RefreshTagsList()
{
    // Update the unselected tags list
    UnselectedTags = GetUnselectedTags();
    
    if (TagListView.IsValid())
    {
        TagListView->RequestListRefresh();
    }
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

FReply SDevNoteTagPicker::OnTagRowMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, TSharedPtr<FDevNoteTag> ClickedTag)
{
    if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        // Create and show context menu for right-click
        TSharedRef<SWidget> MenuWidget = CreateTagContextMenu(ClickedTag);
        
        FSlateApplication::Get().PushMenu(
            AsShared(),
            FWidgetPath(),
            MenuWidget,
            MouseEvent.GetScreenSpacePosition(),
            FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
        );
        
        return FReply::Handled();
    }
    else if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        // Handle normal left-click (existing functionality)
        OnTagClicked(ClickedTag);
        return FReply::Handled();
    }
    
    return FReply::Unhandled();
}

TSharedRef<SWidget> SDevNoteTagPicker::CreateTagContextMenu(TSharedPtr<FDevNoteTag> ClickedTag)
{
    FMenuBuilder MenuBuilder(true, nullptr);
    
    MenuBuilder.AddMenuEntry(
        FText::FromString(TEXT("Delete Tag")),
        FText::FromString(FString::Printf(TEXT("Delete the tag '%s' from the database"), *ClickedTag->Name)),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateSP(this, &SDevNoteTagPicker::OnDeleteTagClicked, ClickedTag))
    );

    return MenuBuilder.MakeWidget();
}

void SDevNoteTagPicker::OnDeleteTagClicked(TSharedPtr<FDevNoteTag> TagToDelete)
{
    if (!TagToDelete.IsValid())
    {
        return;
    }

    // Show confirmation dialog
    const FText ConfirmText = FText::FromString(
        FString::Printf(TEXT("Are you sure you want to delete the tag '%s'?\n\nThis will remove it from all notes and cannot be undone."), 
        *TagToDelete->Name)
    );
    
    EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNo, ConfirmText);

    if (Result == EAppReturnType::Yes)
    {
        if (UDevNoteSubsystem* Subsystem = UDevNoteSubsystem::Get())
        {
            // Delete the tag from the server
            Subsystem->DeleteTag(TagToDelete->Id);
            
            // Close the dropdown menu
            if (TagPickerAnchor.IsValid())
            {
                TagPickerAnchor->SetIsOpen(false);
            }
        }
    }
}
