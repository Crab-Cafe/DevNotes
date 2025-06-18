#include "SDevNoteEditor.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "DevNoteSubsystem.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Editor/EditorEngine.h"
#include "DevNoteActor.h"
#include "FDevNoteTag.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SComboBox.h"
#include "PropertyCustomizationHelpers.h"
#include "SDevNoteTagPicker.h"
#include "Containers/Array.h"


FString SDevNoteEditor::GetLevelPath() const
{
    if (SelectedNote == nullptr) return FString();
    
    return SelectedNote->LevelPath.ToString();
}

void SDevNoteEditor::OnLevelPathChanged(const FAssetData& AssetData)
{
    SelectedNote->LevelPath = AssetData.GetSoftObjectPath();
    UDevNoteSubsystem::Get()->UpdateNote(*SelectedNote);
}

void SDevNoteEditor::OnTagPickerOpened()
{
    UDevNoteSubsystem::Get()->RequestTagsFromServer();
}

void SDevNoteEditor::Construct(const FArguments& InArgs)
{
    SelectedNote = InArgs._SelectedNote;

    TitleText = SelectedNote.IsValid() ? SelectedNote->Title : FString();
    BodyText = SelectedNote.IsValid() ? SelectedNote->Body : FString();

    // Initialize tags list
    RefreshTagsList();

    UDevNoteSubsystem::Get()->OnTagsUpdated.AddSP(SharedThis(this), &SDevNoteEditor::RefreshTagsList);
    
    ChildSlot
    [
        SNew(SVerticalBox)

        // Title (Editable)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FMargin(0, 0, 0, 6))
        [
            SNew(SEditableTextBox)
            .Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
            .Text_Lambda([this]() -> FText {
                return FText::FromString(TitleText);
            })
            .OnTextChanged_Lambda([this](const FText& NewText) {
                TitleText = NewText.ToString();
                UDevNoteSubsystem::Get()->SetEditorEditingState(true);
            })
            .IsEnabled_Lambda([this]() -> bool {
                return SelectedNote.IsValid();
            })
            .HintText(FText::FromString(TEXT("Note Title")))
            .OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type CommitType) {
                if (SelectedNote.IsValid() && (CommitType != ETextCommit::OnCleared))
                {
                    TitleText = NewText.ToString();
                    SelectedNote->Title = TitleText;

                    if (UDevNoteSubsystem* Subsystem = GEditor->GetEditorSubsystem<UDevNoteSubsystem>())
                    {
                        Subsystem->UpdateNote(*SelectedNote);
                    }
                    UDevNoteSubsystem::Get()->SetEditorEditingState(false);
                }
            })
        ]

        // Level Picker
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FMargin(0, 0, 0, 6))
        [
            SNew(SObjectPropertyEntryBox)
                .AllowedClass(UWorld::StaticClass())
                .ObjectPath(this, &SDevNoteEditor::GetLevelPath)
                .OnObjectChanged(this, &SDevNoteEditor::OnLevelPathChanged)
                .AllowClear(true)
                .DisplayUseSelected(true)
                .IsEnabled_Lambda([this]() -> bool {
                     return SelectedNote.IsValid();
                 })
        ]

        // Tag selector
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FMargin(0, 0, 0, 12))
        [
            SAssignNew(TagPicker, SDevNoteTagPicker)
                .AvailableTags(&TagsList)
                .SelectedTagIds(SelectedNote.IsValid() ? &SelectedNote->Tags : nullptr)
                .OnSelectionChanged(this, &SDevNoteEditor::OnTagSelectionChanged)
                .OnNewTagCreated(this, &SDevNoteEditor::OnNewTagCreated)
                .OnTagListOpened(this, &SDevNoteEditor::OnTagPickerOpened)
                .IsEnabled_Lambda([this]() -> bool {
                    return SelectedNote.IsValid();
                })  
        ]
        
        // Details
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(FMargin(0, 0, 0, 12))
        [
            SNew(STextBlock)
            .AutoWrapText(true)
            .Text_Lambda([this]() -> FText {
                if (!SelectedNote.IsValid())
                    return FText::GetEmpty();

                const FVector& Loc = SelectedNote->WorldPosition;

                FDateTime localTimeCreated = (FDateTime::Now().GetTicks() - FDateTime::UtcNow().GetTicks()) + SelectedNote->CreatedAt.GetTicks();
                FDateTime localTimeLastEdited = (FDateTime::Now().GetTicks() - FDateTime::UtcNow().GetTicks()) + SelectedNote->LastEdited.GetTicks();

                FString CreatedAtStr = localTimeCreated.ToString(TEXT("%Y-%m-%d %H:%M:%S"));
                FString LastEditedStr = localTimeLastEdited.ToString(TEXT("%Y-%m-%d %H:%M:%S"));

                FString Details = FString::Printf(
                    TEXT("Location: (%.1f, %.1f, %.1f)\nCreated At: %s\nLast Edited: %s"),
                    Loc.X, Loc.Y, Loc.Z,
                    *CreatedAtStr,
                    *LastEditedStr);

                return FText::FromString(Details);
            })
        ]

        // Body text
        + SVerticalBox::Slot()
        .FillHeight(1.f)
        .MinHeight(100.f)
        .VAlign(VAlign_Fill)
        [
                SNew(SMultiLineEditableTextBox)
                .AutoWrapText(true)
                .Text_Lambda([this]() -> FText {
                    return FText::FromString(BodyText);
                })
                .OnTextChanged_Lambda([this](const FText& NewText) {
                    BodyText = NewText.ToString();
                    UDevNoteSubsystem::Get()->SetEditorEditingState(true);
                })
                .IsEnabled_Lambda([this]() -> bool {
                    return SelectedNote.IsValid();
                })
                .HintText(FText::FromString(TEXT("Note Body")))
                .OnTextCommitted_Lambda([this](const FText& NewText, ETextCommit::Type CommitType) {
                    if (SelectedNote.IsValid() && (CommitType != ETextCommit::OnCleared))
                    {
                        BodyText = NewText.ToString();
                        SelectedNote->Body = BodyText;

                        auto ss = UDevNoteSubsystem::Get();
                        ss->UpdateNote(*SelectedNote);
                        ss->SetEditorEditingState(false);
                    }
                })
        ]

        // Button Row
        + SVerticalBox::Slot()
        .AutoHeight()
        .HAlign(HAlign_Fill)
        .VAlign(VAlign_Bottom)
        .Padding(FMargin(0, 12, 0, 0))
        [
            SNew(SHorizontalBox)

            // Select Button
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SButton)
                .Text(FText::FromString(TEXT("Select")))
                .IsEnabled_Lambda([this]() { return SelectedNote.IsValid(); })
                .OnClicked_Lambda([this]() -> FReply {
                    if (!SelectedNote.IsValid())
                        return FReply::Handled();

                    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
                    if (!World)
                        return FReply::Handled();

                    for (TActorIterator<ADevNoteActor> It(World); It; ++It)
                    {
                        if (It->Note == SelectedNote)
                        {
                            GEditor->SelectNone(false, true, false);
                            GEditor->SelectActor(*It, true, true, true);
                            break;
                        }
                    }
                    return FReply::Handled();
                })
            ]
            
            // Teleport Button
            + SHorizontalBox::Slot()
            .AutoWidth()
            .Padding(FMargin(6, 0))
            [
                SNew(SButton)
                .Text(FText::FromString(TEXT("Teleport")))
                .IsEnabled_Lambda([this]() { return SelectedNote.IsValid(); })
                .OnClicked_Lambda([this]() -> FReply {
                    if (!SelectedNote.IsValid())
                        return FReply::Handled();

                    FVector CamTarget = SelectedNote->WorldPosition;

                    auto ss = GEditor->GetEditorSubsystem<UDevNoteSubsystem>();
                    if (ss)
                    {
                        ss->PromptAndTeleportToNote(*SelectedNote);
                    }

                    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
                    if (!World) return FReply::Handled();

                    for (TActorIterator<ADevNoteActor> It(World); It; ++It)
                    {
                        if (It->Note == SelectedNote)
                        {
                            GEditor->SelectNone(false, true, false);
                            GEditor->SelectActor(*It, true, true, true);
                            break;
                        }
                    }
                    
                    return FReply::Handled();
                })
            ]
            // Spacer takes the rest of the horizontal space
            + SHorizontalBox::Slot()
            .FillWidth(1.f)
            [
                SNew(SSpacer)
            ]
            // Delete button on right hand side
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SButton)
                .Text(FText::FromString(TEXT("Delete")))
                .IsEnabled_Lambda([this]() { return SelectedNote.IsValid(); })
                .OnClicked_Lambda([this]() -> FReply {
                    if (!SelectedNote.IsValid())
                        return FReply::Handled();

                    const FText ConfirmText = FText::FromString(TEXT("Are you sure you want to delete this note? This action cannot be undone."));
                    EAppReturnType::Type Result = FMessageDialog::Open(EAppMsgType::YesNo, ConfirmText);

                    if (Result == EAppReturnType::Yes)
                    {
                        if (GEditor)
                        {
                            if (UDevNoteSubsystem* Subsystem = GEditor->GetEditorSubsystem<UDevNoteSubsystem>())
                            {
                                Subsystem->DeleteNote(SelectedNote->Id);

                                UWorld* World = GEditor->GetEditorWorldContext().World();
                                if (World)
                                {
                                    for (TActorIterator<ADevNoteActor> It(World); It; ++It)
                                    {
                                        if (It->Note == SelectedNote)
                                        {
                                            It->Destroy();
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                        SetSelectedNote(nullptr);
                    }
                    return FReply::Handled();
                })
            ]
        ]
    ];
}

void SDevNoteEditor::SetSelectedNote(TSharedPtr<FDevNote> InNote)
{
    SelectedNote = InNote;
    TitleText = SelectedNote.IsValid() ? SelectedNote->Title : FString();
    BodyText = SelectedNote.IsValid() ? SelectedNote->Body : FString();

    // Update tag picker with new note's tags
    if (TagPicker.IsValid())
    {
        TagPicker->UpdateSelectedTags();
        TagPicker->SetSelectedTagIDs(SelectedNote.IsValid() ? &SelectedNote->Tags : nullptr);
    }

    if (this->IsConstructed())
    {
        this->Invalidate(EInvalidateWidget::LayoutAndVolatility);
    }
}

void SDevNoteEditor::OnTagSelectionChanged(const TArray<FGuid>& NewTagIds)
{
    if (SelectedNote.IsValid())
    {
        SelectedNote->Tags = NewTagIds;
        
        if (UDevNoteSubsystem* Subsystem = UDevNoteSubsystem::Get())
        {
            Subsystem->UpdateNote(*SelectedNote);
        }
    }
}

void SDevNoteEditor::OnNewTagCreated(const FDevNoteTag& NewTag)
{
    if (UDevNoteSubsystem* Subsystem = UDevNoteSubsystem::Get())
    {
        Subsystem->PostTag(NewTag);
        
        // Refresh our local tags list after a short delay to allow server sync
        FTimerHandle RefreshTimerHandle;
        GEditor->GetTimerManager()->SetTimer(RefreshTimerHandle, [this]() {
            RefreshTagsList();
            if (TagPicker.IsValid())
            {
                TagPicker->RefreshTagsList();
            }
        }, 0.5f, false);
    }
}

void SDevNoteEditor::RefreshTagsList()
{
    TagsList.Empty();
    if (UDevNoteSubsystem* Subsystem = UDevNoteSubsystem::Get())
    {
        const TArray<FDevNoteTag>& CachedTags = Subsystem->GetCachedTags();
        for (const FDevNoteTag& NoteTag : CachedTags)
        {
            TagsList.Add(MakeShared<FDevNoteTag>(NoteTag));
        }
    }
}