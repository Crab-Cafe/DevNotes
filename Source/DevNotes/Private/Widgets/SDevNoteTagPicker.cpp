#include "SDevNoteTagPicker.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Colors/SColorPicker.h"

void SDevNoteTagPicker::Construct(const FArguments& InArgs)
{
	TagsList = InArgs._AvailableTags;
	SelectedTagIds = InArgs._SelectedTagIds;
	OnSelectionChanged = InArgs._OnSelectionChanged;
	OnNewTagCreated = InArgs._OnNewTagCreated;

	NewTagName.Empty();
	NewTagColor = 0;

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SAssignNew(TagPickerAnchor, SMenuAnchor)
			.OnGetMenuContent(this, &SDevNoteTagPicker::GenerateTagDropdown)
			[
				SNew(SButton)
				.Text(this, &SDevNoteTagPicker::GetSelectedTagsText)
				.OnClicked(this, &SDevNoteTagPicker::OnTagButtonClicked)
			]
		]
	];
}

FReply SDevNoteTagPicker::OnTagButtonClicked()
{
	TagPickerAnchor->SetIsOpen(!TagPickerAnchor->IsOpen());
	return FReply::Handled();
}

TSharedRef<SWidget> SDevNoteTagPicker::GenerateTagDropdown()
{
	auto StrongShared = AsShared();
	return SNew(SBorder).Padding(8)
	[
		SNew(SVerticalBox)
		// Tag List
		+ SVerticalBox::Slot().AutoHeight().MaxHeight(160)
		[
			SAssignNew(TagListView, SListView<TSharedPtr<FDevNoteTag>>)
			.ListItemsSource(TagsList)
			.OnGenerateRow(this, &SDevNoteTagPicker::GenerateTagRow)
			.SelectionMode(ESelectionMode::Multi)
			.OnSelectionChanged(this, &SDevNoteTagPicker::OnTagSelectionChangedInternal)
		]
		// New Tag Input
		+ SVerticalBox::Slot().AutoHeight().Padding(0,8,0,0)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SAssignNew(NewTagNameBox, SEditableTextBox)
				.HintText(FText::FromString(TEXT("New tag name")))
				.Text(this, &SDevNoteTagPicker::GetNewTagNameText)
				.OnTextChanged(this, &SDevNoteTagPicker::OnNewTagNameChanged)
			]
			
			+ SHorizontalBox::Slot().AutoWidth().Padding(4,0)
			[
				SNew(SButton)
				.ButtonStyle(FAppStyle::Get(), "NoBorder")
				.OnClicked_Lambda([StrongShared]() {
					TSharedRef<SWindow> PickerWindow = SNew(SWindow)
					.Title(FText::FromString("Pick a Color"))
					.ClientSize(FVector2D(300, 400))
					.SupportsMaximize(false)
					.SupportsMinimize(false)
					.HasCloseButton(true)
					.IsTopmostWindow(true) // Optional
					.CreateTitleBar(true);

				PickerWindow->SetContent(
					SNew(SColorPicker)
					.DisplayInlineVersion(true)
					.OnColorCommitted(FOnLinearColorValueChanged::CreateLambda([StrongShared](FLinearColor NewColor) {
						//StrongShared->PendingTagColor = NewColor;
					})));
					
					FSlateApplication::Get().AddWindowAsNativeChild(PickerWindow, FSlateApplication::Get().FindWidgetWindow(StrongShared).ToSharedRef(), false);
					return FReply::Handled();
				})
				[
					SAssignNew(ColorBlockWidget, SColorBlock)
					.Color(this, &SDevNoteTagPicker::GetPendingTagColor)
					.Size(FVector2D(24, 24))
					.AlphaDisplayMode(EColorBlockAlphaDisplayMode::Ignore)
				]
			]


			+ SHorizontalBox::Slot().AutoWidth().Padding(4,0)
			[
				SNew(SButton)
				.Text(FText::FromString(TEXT("Add")))
				.OnClicked(this, &SDevNoteTagPicker::OnAddNewTagClicked)
				.IsEnabled(this, &SDevNoteTagPicker::IsNewTagValid)
			]
		]
	];
}

TSharedRef<ITableRow> SDevNoteTagPicker::GenerateTagRow(TSharedPtr<FDevNoteTag> InTag, const TSharedRef<STableViewBase>& OwnerTable)
{
	FLinearColor TagColor = FLinearColor::FromSRGBColor(FColor::FromHex(FString::Printf(TEXT("%06X"), InTag->Colour)));
	return SNew(STableRow<TSharedPtr<FDevNoteTag>>, OwnerTable)
	[
		SNew(STextBlock)
		.Text(FText::FromString(InTag->Name))
		.ColorAndOpacity(TagColor)
	];
}



void SDevNoteTagPicker::OnTagSelectionChangedInternal(TSharedPtr<FDevNoteTag>, ESelectInfo::Type)
{
	if (!SelectedTagIds || !TagListView.IsValid()) return;

	SelectedTagIds->Empty();
	TArray<TSharedPtr<FDevNoteTag>> SelectedItems;
	TagListView->GetSelectedItems(SelectedItems);
	for (const auto& NoteTag : SelectedItems)
	{
		SelectedTagIds->Add(NoteTag->Id);
	}
	OnSelectionChanged.ExecuteIfBound(*SelectedTagIds);
}

FText SDevNoteTagPicker::GetSelectedTagsText() const
{
	if (!SelectedTagIds || !TagsList || SelectedTagIds->Num() == 0)
		return FText::FromString(TEXT("None"));
	FString Result;
	for (auto& NoteTag : *TagsList)
	{
		if (SelectedTagIds->Contains(NoteTag->Id))
		{
			if (!Result.IsEmpty()) Result += TEXT(", ");
			Result += NoteTag->Name;
		}
	}
	return FText::FromString(Result);
}

FText SDevNoteTagPicker::GetNewTagNameText() const { return FText::FromString(NewTagName); }
void SDevNoteTagPicker::OnNewTagNameChanged(const FText& NewText) { NewTagName = NewText.ToString(); }
FText SDevNoteTagPicker::GetNewTagColorText() const { return (NewTagColor > 0) ? FText::FromString(FString::Printf(TEXT("%06X"), NewTagColor)) : FText::GetEmpty(); }
bool SDevNoteTagPicker::IsNewTagValid() const { return !NewTagName.IsEmpty(); }
FReply SDevNoteTagPicker::OnAddNewTagClicked()
{
	FDevNoteTag NewTag;
	NewTag.Id = FGuid::NewGuid();
	NewTag.Name = NewTagName;
	FColor NewColor = PendingTagColor.ToFColor(true);
	int32 ColorAsInt = (int32)NewColor.ToPackedARGB(); // Choose BGRA or RGBA to match any usage
	NewTag.Colour = ColorAsInt;
	if (TagsList)
	{
		TagsList->Add(MakeShared<FDevNoteTag>(NewTag));
		if (TagListView.IsValid())
			TagListView->RequestListRefresh();
	}
	OnNewTagCreated.ExecuteIfBound(NewTag);

	NewTagName.Empty();
	NewTagColor = 0;
	if (NewTagNameBox.IsValid()) NewTagNameBox->SetText(FText::GetEmpty());

	return FReply::Handled();
}