#include "SDevNoteSelector.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Text/STextBlock.h"

void SDevNoteSelector::Construct(const FArguments& InArgs)
{
    NoteSelectedDelegate = InArgs._OnNoteSelected;
    OnRefreshNotes = InArgs._OnRefreshNotes;
    OnNewNote = InArgs._OnNewNote;

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SButton)
                .Text(FText::FromString(TEXT("Refresh")))
                .OnClicked(this, &SDevNoteSelector::OnRefreshClicked)
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            [
                SNew(SButton)
                .Text(FText::FromString(TEXT("Add")))
                .OnClicked(this, &SDevNoteSelector::OnNewNoteClicked)
            ]
        ]
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SAssignNew(NotesListView, SListView<TSharedPtr<FDevNote>>)
            .ListItemsSource(&Notes)
            .ReturnFocusToSelection(true)
            .OnGenerateRow(this, &SDevNoteSelector::OnGenerateNoteRow)
            .OnSelectionChanged(this, &SDevNoteSelector::OnNoteSelectedInternal)
            .SelectionMode(ESelectionMode::Single)
        ]
    ];
}


void SDevNoteSelector::SetNotesSource(const TArray<TSharedPtr<FDevNote>>& InNotes)
{
    Notes.Empty();
    for (auto& Note : InNotes)
    {
        Notes.Add(Note);
    }

    if (NotesListView.IsValid())
    {
        NotesListView->RequestListRefresh();
    }
}

void SDevNoteSelector::SetSelectedNote(const TSharedPtr<FDevNote>& InNote)
{
    if (NotesListView.IsValid())
    {
        NotesListView->SetSelection(InNote);
    }
}

TSharedRef<ITableRow> SDevNoteSelector::OnGenerateNoteRow(
    TSharedPtr<FDevNote> InNote,
    const TSharedRef<STableViewBase>& OwnerTable)
{
    return SNew(STableRow<TSharedPtr<FDevNote>>, OwnerTable)
    [
        SNew(STextBlock).Text(FText::FromString(InNote->Title))
    ];
}

void SDevNoteSelector::OnNoteSelectedInternal(TSharedPtr<FDevNote> InNote, ESelectInfo::Type SelectionType)
{
    if (NoteSelectedDelegate.IsBound())
    {
        NoteSelectedDelegate.Execute(InNote);
    }
}

FReply SDevNoteSelector::OnRefreshClicked()
{
    if (OnRefreshNotes.IsBound())
    {
        OnRefreshNotes.Execute();
    }
    
    return FReply::Handled();
}

FReply SDevNoteSelector::OnNewNoteClicked()
{
    if (OnNewNote.IsBound())
    {
        OnNewNote.Execute();
    }
    return FReply::Handled();
}
