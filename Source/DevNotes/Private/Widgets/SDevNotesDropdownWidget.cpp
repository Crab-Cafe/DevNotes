#include "SDevNotesDropdownWidget.h"

#include "DevNoteSubsystem.h"
#include "Kismet/KismetTextLibrary.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"

FReply SDevNotesDropdownWidget::RefreshNotes()
{
	if (GEngine)
	{
		if (UDevNoteSubsystem* Subsystem = GEngine->GetEngineSubsystem<UDevNoteSubsystem>())
		{
			Subsystem->RequestNotesFromServer();
		}
	}
	return FReply::Handled();
}

FReply SDevNotesDropdownWidget::NewNote()
{
	if (GEngine)
	{
		if (UDevNoteSubsystem* Subsystem = GEngine->GetEngineSubsystem<UDevNoteSubsystem>())
		{
			Subsystem->CreateNewNoteAtEditorLocation();
		}
	}
	return FReply::Handled();
}

void SDevNotesDropdownWidget::Construct(const FArguments& InArgs)
{
	if (GEngine)
	{
		UDevNoteSubsystem* Subsystem = GEngine->GetEngineSubsystem<UDevNoteSubsystem>();
		if (Subsystem)
		{
			Subsystem->OnNotesUpdated.AddSP(SharedThis(this), &SDevNotesDropdownWidget::OnNotesUpdated);
		}
	}

	ChildSlot
	[
		SNew(SSplitter)
		+ SSplitter::Slot()
		.Value(0.3f)
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
					.OnClicked(this, &SDevNotesDropdownWidget::RefreshNotes)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(FText::FromString(TEXT("Add")))
					.OnClicked(this, &SDevNotesDropdownWidget::NewNote)
				]
			]

			+SVerticalBox::Slot()
			.FillHeight(1.0)
			[
				SAssignNew(NotesListView, SListView<TSharedPtr<FDevNote>>)
				.ListItemsSource(&Notes)
				.OnGenerateRow(this, &SDevNotesDropdownWidget::OnGenerateNoteRow)
				.OnSelectionChanged(this, &SDevNotesDropdownWidget::OnNoteSelected)
				.SelectionMode(ESelectionMode::Single)
			]
		]

		+ SSplitter::Slot()
		.Value(0.7f)
		[
			SNew(SScrollBox)

			+ SScrollBox::Slot()
			.Padding(8)
			[
				SNew(SVerticalBox)

				// Title
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0, 0, 0, 6))
				[
					SNew(STextBlock)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
					.Text_Lambda([this]() -> FText {
						return SelectedNote.IsValid()
							? FText::FromString(SelectedNote->Title)
							: FText::FromString(TEXT("Select a note"));
					})
				]

				// Details (location, level, created at)
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
						const FString& Level = SelectedNote->LevelPath;

						FDateTime localTime = (FDateTime::Now().GetTicks() - FDateTime::UtcNow().GetTicks()) + SelectedNote->CreatedAt.GetTicks();
						
						FString CreatedAtStr = localTime.ToString(TEXT("%Y-%m-%d %H:%M:%S"));
	
						FString Details = FString::Printf(
							TEXT("Location: (%.1f, %.1f, %.1f)\nLevel: %s\nCreated At: %s"),
							Loc.X, Loc.Y, Loc.Z,
							*Level,
							*CreatedAtStr);

						return FText::FromString(Details);
					})
				]

				// Body text
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.AutoWrapText(true)
					.Text_Lambda([this]() -> FText {
						return SelectedNote.IsValid()
							? FText::FromString(SelectedNote->Body)
							: FText::FromString(TEXT("Select a note to view its details."));
					})
				]
			]
		]
	];
}

TSharedRef<ITableRow> SDevNotesDropdownWidget::OnGenerateNoteRow(
	TSharedPtr<FDevNote> InNote,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FDevNote>>, OwnerTable)
	[
		SNew(STextBlock).Text(FText::FromString(InNote->Title))
	];
}

void SDevNotesDropdownWidget::OnNoteSelected(TSharedPtr<FDevNote> InNote, ESelectInfo::Type /*SelectInfo*/)
{
	SelectedNote = InNote;
}

void SDevNotesDropdownWidget::SetNotesSource(const TArray<FDevNote>& InNotes)
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

void SDevNotesDropdownWidget::OnNotesUpdated()
{
	if (GEngine)
	{
		if (UDevNoteSubsystem* Subsystem = GEngine->GetEngineSubsystem<UDevNoteSubsystem>())
		{
			SetNotesSource(Subsystem->GetNotes());
		}
	}
}
