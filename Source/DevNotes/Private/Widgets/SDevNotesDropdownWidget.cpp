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
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SEditableTextBox.h"

#define LOCTEXT_NAMESPACE "SDevNotesDropdownWidget"

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

	bIsLoggedIn = false;
	ErrorMsg = FString();

	ChildSlot
	.Padding(10)
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 8)
		[
			SAssignNew(LoginStatusBox, SHorizontalBox)
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
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
		]
	];

	// Attempt auto sign-in
	TryUpdateLoginStatus();
}

void SDevNotesDropdownWidget::TryUpdateLoginStatus()
{
	if (GEditor)
	{
		if (UDevNoteSubsystem* Subsystem = GEditor->GetEditorSubsystem<UDevNoteSubsystem>())
		{
			bIsLoggedIn = Subsystem->TryAutoSignIn();
			UpdateLoginStatusBox();
		}
	}
}

void SDevNotesDropdownWidget::UpdateLoginStatusBox()
{
	LoginStatusBox->ClearChildren();

	if (bIsLoggedIn)
	{
		LoginStatusBox->AddSlot()
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("LoggedInStatus", "Status: Logged in"))
		];

		LoginStatusBox->AddSlot()
		.AutoWidth()
		.Padding(10,0)
		[
			SNew(SButton)
			.Text(LOCTEXT("LogoutButton", "Log Out"))
			.OnClicked(this, &SDevNotesDropdownWidget::OnLogoutClicked)
		];
	}
	else
	{
		LoginStatusBox->AddSlot()
		.AutoWidth()
		.Padding(0,0,8,0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("LoggedOutStatus", "Status: Not logged in"))
		];

		LoginStatusBox->AddSlot()
		.FillWidth(1.0f)
		[
			SNew(SEditableTextBox)
			.MinDesiredWidth(80)
			.HintText(LOCTEXT("UsernameHint", "Username"))
			.OnTextCommitted(this, &SDevNotesDropdownWidget::OnUsernameCommitted)
			// Keep the latest entered
			// .Text_Raw(this, &SDevNotesDropdownWidget::GetCachedUsernameText)
			// .OnTextChanged(this, &SDevNotesDropdownWidget::OnUsernameChanged)
		];

		LoginStatusBox->AddSlot()
		.FillWidth(1.0f)
		[
			SNew(SEditableTextBox)
			.MinDesiredWidth(80)
			.HintText(LOCTEXT("PasswordHint", "Password"))
			.IsPassword(true)
			.OnTextCommitted(this, &SDevNotesDropdownWidget::OnPasswordCommitted)
			// .Text_Raw(this, &SDevNotesDropdownWidget::GetCachedPasswordText)
			// .OnTextChanged(this, &SDevNotesDropdownWidget::OnPasswordChanged)
		];

		LoginStatusBox->AddSlot()
		.AutoWidth()
		.Padding(5,0)
		[
			SNew(SButton)
			.Text(LOCTEXT("LoginButton", "Log In"))
			.OnClicked(this, &SDevNotesDropdownWidget::OnLoginClicked)
		];
	}

	if (!ErrorMsg.IsEmpty())
	{
		LoginStatusBox->AddSlot()
		.AutoWidth()
		.Padding(10,0)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FLinearColor::Red)
			.Text(FText::FromString(ErrorMsg))
		];
	}
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

// ---------- Login/logout support ----------

FReply SDevNotesDropdownWidget::OnLoginClicked()
{
	ErrorMsg.Empty();
	if (CachedUsername.IsEmpty() || CachedPassword.IsEmpty())
	{
		ErrorMsg = TEXT("Please enter both username and password.");
		UpdateLoginStatusBox();
		return FReply::Handled();
	}
	if (GEditor)
	{
		if (UDevNoteSubsystem* Subsystem = GEditor->GetEditorSubsystem<UDevNoteSubsystem>())
		{
			Subsystem->SignIn(
				CachedUsername, CachedPassword,
				[this](bool bSuccess, const FString& Error) {
					this->OnLoginResponse(bSuccess, Error);
				}
			);

		}
	}
	return FReply::Handled();
}

void SDevNotesDropdownWidget::OnLoginResponse(bool bSuccess, const FString& Error)
{
	bIsLoggedIn = bSuccess;
	ErrorMsg = bSuccess ? FString() : Error;
	UpdateLoginStatusBox();
	if (bSuccess)
	{
		RefreshNotes();
	}
}

FReply SDevNotesDropdownWidget::OnLogoutClicked()
{
	ErrorMsg.Empty();
	if (GEditor)
	{
		if (UDevNoteSubsystem* Subsystem = GEditor->GetEditorSubsystem<UDevNoteSubsystem>())
		{
			Subsystem->SignOut(
	[this](bool bSuccess) {
		this->OnLogoutResponse(bSuccess);
	}
);

		}
	}
	return FReply::Handled();
}

void SDevNotesDropdownWidget::OnLogoutResponse(bool bSuccess)
{
	bIsLoggedIn = false;
	ErrorMsg = FString();
	UpdateLoginStatusBox();
}

void SDevNotesDropdownWidget::OnUsernameCommitted(const FText& Text, ETextCommit::Type /*CommitType*/)
{
	CachedUsername = Text.ToString();
}

void SDevNotesDropdownWidget::OnPasswordCommitted(const FText& Text, ETextCommit::Type /*CommitType*/)
{
	CachedPassword = Text.ToString();
}

#undef LOCTEXT_NAMESPACE