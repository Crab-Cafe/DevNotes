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
            .OnGenerateRow(this, &SDevNoteSelector::OnGenerateNoteRow)
            .OnSelectionChanged(this, &SDevNoteSelector::OnNoteSelectedInternal)
            .SelectionMode(ESelectionMode::Single)
        ]
    ];
}

void SDevNoteSelector::SetNotesSource(const TArray<FDevNote>& InNotes)
{
    Notes.Empty();
    for (const FDevNote& Note : InNotes)
    {
        Notes.Add(MakeShared<FDevNote>(Note));
    }

    if (NotesListView.IsValid())
    {
        NotesListView->RequestListRefresh();
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

void SDevNoteSelector::OnNoteSelectedInternal(TSharedPtr<FDevNote> InNote, ESelectInfo::Type)
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
