#include "SDevNotesDropdownWidget.h"

#include "DevNoteSubsystem.h"
#include "SDevNoteEditor.h"
#include "SDevNoteSelector.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"

void SDevNotesDropdownWidget::RefreshNotes()
{
	if (GEditor)
	{
		if (UDevNoteSubsystem* Subsystem = GEditor->GetEditorSubsystem<UDevNoteSubsystem>())
		{
			Subsystem->RequestNotesFromServer();
		}
	}
}

void SDevNotesDropdownWidget::NewNote()
{
	if (GEditor)
	{
		if (UDevNoteSubsystem* Subsystem = GEditor->GetEditorSubsystem<UDevNoteSubsystem>())
		{
			Subsystem->CreateNewNoteAtEditorLocation();
		}
	}
}

void SDevNotesDropdownWidget::Construct(const FArguments& InArgs)
{
	if (GEditor)
	{
		UDevNoteSubsystem* Subsystem = GEditor->GetEditorSubsystem<UDevNoteSubsystem>();
		if (Subsystem)
		{
			Subsystem->OnNotesUpdated.AddSP(SharedThis(this), &SDevNotesDropdownWidget::OnNotesUpdated);
		}
	}

	ChildSlot
	.Padding(10)
	[
		SNew(SSplitter)
		+ SSplitter::Slot()
		.Value(0.3f)
		[
			SNew(SBox)
			.Padding(0,0,5,0)
			[
				SAssignNew(Selector, SDevNoteSelector)
				.OnNoteSelected(this, &SDevNotesDropdownWidget::OnNoteSelected)
				.OnRefreshNotes(this, &SDevNotesDropdownWidget::RefreshNotes)
				.OnNewNote(this, &SDevNotesDropdownWidget::NewNote)
			]
		]
		+ SSplitter::Slot()
		.Value(0.7f)
		[
			SNew(SBox)
			.Padding(5, 0, 0, 0)
			[
				SAssignNew(Editor, SDevNoteEditor)
				.SelectedNote(SelectedNote)
			]
		]
	];
}

void SDevNotesDropdownWidget::OnNoteSelected(TSharedPtr<FDevNote> InNote)
{
	SelectedNote = InNote;
	Editor->SetSelectedNote(SelectedNote);
	SelectedNoteId = (InNote.IsValid()) ? InNote->Id : FGuid();
}

void SDevNotesDropdownWidget::SetNotesSource(const TArray<TSharedPtr<FDevNote>>& InNotes)
{
	Selector->SetNotesSource(InNotes);

	if (SelectedNoteId.IsValid())
	{
		TSharedPtr<FDevNote> Found;
		for (const TSharedPtr<FDevNote>& Note : InNotes)
		{
			if (Note.IsValid() && Note->Id == SelectedNoteId)
			{
				Found = Note;
				break;
			}
		}
		SelectedNote = Found;
	}
	else
	{
		SelectedNote.Reset();
	}
	
	Editor->SetSelectedNote(SelectedNote);
	Selector->SetSelectedNote(SelectedNote);
}

void SDevNotesDropdownWidget::OnNotesUpdated()
{
	if (GEditor)
	{
		if (UDevNoteSubsystem* Subsystem = GEditor->GetEditorSubsystem<UDevNoteSubsystem>())
		{
			SetNotesSource(Subsystem->GetNotes());
		}
	}
}
