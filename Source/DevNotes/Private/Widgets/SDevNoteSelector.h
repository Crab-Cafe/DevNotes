#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "FDevNote.h"

DECLARE_DELEGATE_OneParam(FOnDevNoteSelected, TSharedPtr<FDevNote>);
DECLARE_DELEGATE(FOnRefreshNotes);
DECLARE_DELEGATE(FOnNewNote);

class SDevNoteSelector : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDevNoteSelector) {}
	SLATE_EVENT(FOnDevNoteSelected, OnNoteSelected)
	SLATE_EVENT(FOnRefreshNotes, OnRefreshNotes)
	SLATE_EVENT(FOnNewNote, OnNewNote)
SLATE_END_ARGS()

void Construct(const FArguments& InArgs);

	void SetNotesSource(const TArray<FDevNote>& InNotes);

private:
	TArray<TSharedPtr<FDevNote>> Notes;
	TSharedPtr<SListView<TSharedPtr<FDevNote>>> NotesListView;
	FOnDevNoteSelected NoteSelectedDelegate;

	TSharedRef<ITableRow> OnGenerateNoteRow(TSharedPtr<FDevNote>, const TSharedRef<STableViewBase>&);
	void OnNoteSelected(TSharedPtr<FDevNote>, ESelectInfo::Type);
	FReply OnRefreshClicked();
	FReply OnNewNoteClicked();

	FOnRefreshNotes OnRefreshNotes;
	FOnNewNote OnNewNote;


	void OnNoteSelectedInternal(TSharedPtr<FDevNote> InNote, ESelectInfo::Type);

};
