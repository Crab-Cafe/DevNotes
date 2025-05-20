#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "FDevNote.h"


class SDevNotesDropdownWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDevNotesDropdownWidget) {}
	SLATE_END_ARGS()

	
	void SetNotesSource(const TArray<FDevNote>& InNotes);
	void OnNotesUpdated();
	void Construct(const FArguments& InArgs);
	FReply RefreshNotes();
private:
	FReply NewNote();

	TArray<TSharedPtr<FDevNote>> Notes;
	TSharedPtr<FDevNote> SelectedNote;

	TSharedPtr<SListView<TSharedPtr<FDevNote>>> NotesListView;

	TSharedRef<ITableRow> OnGenerateNoteRow(TSharedPtr<FDevNote> InNote, const TSharedRef<STableViewBase>& OwnerTable);
	void OnNoteSelected(TSharedPtr<FDevNote> InNote, ESelectInfo::Type SelectInfo);
};

