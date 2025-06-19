#include "SDevNotesDropdownWidget.h"

#include "DevNotesLog.h"
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
			Subsystem->RequestTagsFromServer();
		}
	}
}

void SDevNotesDropdownWidget::OnSignedIn(FString Token)
{
	// Enable editing
	Editor->SetEnabled(true);
	Selector->SetEnabled(true);
	UpdateLoginStatusBox();
}

void SDevNotesDropdownWidget::OnSignedOut()
{
	// Disable editing
	Editor->SetEnabled(false);
	Selector->SetEnabled(false);
	UpdateLoginStatusBox();
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
			// Bind to authentication and note events in subsystem
			Subsystem->OnNotesUpdated.AddSP(SharedThis(this), &SDevNotesDropdownWidget::OnNotesUpdated);
			Subsystem->OnSignedIn.AddSP(SharedThis(this), &SDevNotesDropdownWidget::OnSignedIn);
			Subsystem->OnSignedOut.AddSP(SharedThis(this), &SDevNotesDropdownWidget::OnSignedOut);
		}
	}
	
	bIsLoggedIn = false;
	ErrorMsg = FString();

	ChildSlot
	.Padding(10)
	[
		SNew(SVerticalBox)

		// Login Box
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 8)
		[
			SAssignNew(LoginStatusBox, SHorizontalBox)
		]

		// Splitter
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SSplitter)

			// Devnote Selector
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
			
			// Devnote Editor
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
			bIsLoggedIn = Subsystem->IsLoggedIn();
			if (!bIsLoggedIn)
			{
				bIsLoggedIn = Subsystem->TryAutoSignIn();
			}
			Editor->SetEnabled(bIsLoggedIn);
			Selector->SetEnabled(bIsLoggedIn);
			UpdateLoginStatusBox();
		}
	}
}

void SDevNotesDropdownWidget::UpdateLoginStatusBox()
{
	LoginStatusBox->ClearChildren();

	if (bIsLoggedIn)
	{
		// Status
		LoginStatusBox->AddSlot()
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(FText::Format(LOCTEXT("LoggedInStatus", "Status: Logged in as {0}"), FText::FromString(UDevNoteSubsystem::Get()->GetCurrentUser().Name)))
		];

		// Logout Button
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
		// Status
		LoginStatusBox->AddSlot()
		.AutoWidth()
		.Padding(0,0,8,0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("LoggedOutStatus", "Status: Not logged in"))
		];

		// Username Box
		LoginStatusBox->AddSlot()
		.FillWidth(1.0f)
		[
			SNew(SEditableTextBox)
			.MinDesiredWidth(80)
			.HintText(LOCTEXT("UsernameHint", "Username"))
			.OnTextCommitted(this, &SDevNotesDropdownWidget::OnUsernameCommitted)
			// .Text_Raw(this, &SDevNotesDropdownWidget::GetCachedUsernameText)
			// .OnTextChanged(this, &SDevNotesDropdownWidget::OnUsernameChanged)
		];

		// Password box
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

		// Login button
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
		// Error Message
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
	// Pass note source to selector
	Selector->SetNotesSource(InNotes);

	// Re-select previously selected note if it exists
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
	
	// Propagate note selection to sub-widgets
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

FReply SDevNotesDropdownWidget::OnLoginClicked()
{
	ErrorMsg.Empty();
	if (CachedUsername.IsEmpty() || CachedPassword.IsEmpty())
	{
		ErrorMsg = TEXT("Please enter both username and password.");
		UpdateLoginStatusBox();
		return FReply::Handled();
	}

	// Issue sign in command via subsystem
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
	// Issue logout command via subsystem
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

void SDevNotesDropdownWidget::OnUsernameCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	CachedUsername = Text.ToString();
}

void SDevNotesDropdownWidget::OnPasswordCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	CachedPassword = Text.ToString();
}

#undef LOCTEXT_NAMESPACE