#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "FDevNote.h"


class SDevNoteSelector;
class SDevNoteEditor;

class SDevNotesDropdownWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDevNotesDropdownWidget) {}
	SLATE_END_ARGS()

	
	void SetNotesSource(const TArray<TSharedPtr<FDevNote>>& InNotes);
	void OnNotesUpdated();
	void Construct(const FArguments& InArgs);
	void RefreshNotes();
private:
	void NewNote();

	TSharedPtr<FDevNote> SelectedNote;
	FGuid SelectedNoteId;
	TArray<TSharedPtr<FDevNote>> NotesSource;

	TSharedPtr<SDevNoteEditor> Editor;
	TSharedPtr<SDevNoteSelector> Selector;

	void OnNoteSelected(TSharedPtr<FDevNote> InNote);
};

