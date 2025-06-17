#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "FDevNote.h"


class SDevNoteSelector;
class SDevNoteEditor;


DECLARE_DELEGATE_TwoParams(FOnSignInComplete, bool, const FString&);
DECLARE_DELEGATE_OneParam(FOnSignOutComplete, bool);

class SDevNotesDropdownWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDevNotesDropdownWidget) {}
	SLATE_END_ARGS()

	
	void SetNotesSource(const TArray<TSharedPtr<FDevNote>>& InNotes);
	void OnNotesUpdated();
	void Construct(const FArguments& InArgs);
	void RefreshNotes();

	void OnSignedIn(FString Token);
	void OnSignedOut();
private:
	void NewNote();

	TSharedPtr<FDevNote> SelectedNote;
	FGuid SelectedNoteId;
	TArray<TSharedPtr<FDevNote>> NotesSource;

	TSharedPtr<SDevNoteEditor> Editor;
	TSharedPtr<SDevNoteSelector> Selector;

	bool bIsLoggedIn = false;
	FString CachedUsername, CachedPassword, ErrorMsg;
	TSharedPtr<SHorizontalBox> LoginStatusBox;

	void TryUpdateLoginStatus();
	void UpdateLoginStatusBox();
	FReply OnLoginClicked();
	FReply OnLogoutClicked();
	void OnLoginResponse(bool bSuccess, const FString& Error);
	void OnLogoutResponse(bool bSuccess);
	void OnUsernameCommitted(const FText& Text, ETextCommit::Type CommitType);
	void OnPasswordCommitted(const FText& Text, ETextCommit::Type CommitType);



	void OnNoteSelected(TSharedPtr<FDevNote> InNote);
};

