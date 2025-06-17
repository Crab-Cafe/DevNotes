#include "SDevNoteSelector.h"

#include "StructUtils/PropertyBag.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Text/STextBlock.h"

// Add this helper for parsing tokens with quotes
static void ParseSearchStringWithQuotes(const FString& InStr, TArray<FString>& OutTokens)
{
    int32 Len = InStr.Len();
    bool bInQuotes = false;
    FString Current;
    for (int32 i = 0; i < Len; ++i)
    {
        TCHAR C = InStr[i];
        if (C == '\"')
        {
            bInQuotes = !bInQuotes;
            continue;
        }
        if (!bInQuotes && FChar::IsWhitespace(C))
        {
            if (!Current.IsEmpty())
            {
                OutTokens.Add(Current);
                Current.Empty();
            }
            continue;
        }
        Current.AppendChar(C);
    }
    if (!Current.IsEmpty()) OutTokens.Add(Current);
}

void SDevNoteSelector::OnSearchTextChanged(const FText& Text)
{
    SearchText = Text;
    ParseAndApplyFilters();
}

void SDevNoteSelector::ParseAndApplyFilters()
{
    // Field -> Multiple allowed values (OR logic)
    TMap<FString, TArray<FString>> FieldFilters;
    TArray<FString> GenericTerms;

    TArray<FString> Tokens;
    ParseSearchStringWithQuotes(SearchText.ToString(), Tokens);

    for (const FString& Token : Tokens)
    {
        FString Key, Value;
        if (Token.Split(TEXT("="), &Key, &Value))
        {
            Key = Key.ToLower().TrimStartAndEnd();
            Value = Value.TrimQuotes().TrimStartAndEnd(); // Remove quotes from value if any
            FieldFilters.FindOrAdd(Key).Add(Value);
        }
        else
        {
            GenericTerms.Add(Token.TrimQuotes().TrimStartAndEnd());
        }
    }

    FilteredNotes.Empty();
    for (const auto& Note : Notes)
    {
        bool bPass = true;
        // Name
        if (FieldFilters.Contains("name"))
        {
            bool bAny = false;
            for (const FString& Val : FieldFilters["name"])
            {
                if (Note->Title.Contains(Val, ESearchCase::IgnoreCase))
                {
                    bAny = true; break;
                }
            }
            if (!bAny) { bPass = false; }
        }
        // User (if you have it)
        /*
        if (FieldFilters.Contains("user"))
        {
            bool bAny = false;
            for (const FString& Val : FieldFilters["user"])
            {
                if (Note->User.Contains(Val, ESearchCase::IgnoreCase))
                {
                    bAny = true; break;
                }
            }
            if (!bAny) { bPass = false; }
        }
        */
        // Map
        if (FieldFilters.Contains("map"))
        {
            bool bAny = false;
            for (const FString& Val : FieldFilters["map"])
            {
                if (Note->LevelPath.ToString().Contains(Val, ESearchCase::IgnoreCase))
                {
                    bAny = true; break;
                }
            }
            if (!bAny) { bPass = false; }
        }
        // Tag
        /*
        if (FieldFilters.Contains("tag"))
        {
            bool bAny = false;
            for (const FString& Val : FieldFilters["tag"])
            {
                if (Note->Tags.Contains(Val, ESearchCase::IgnoreCase))
                {
                    bAny = true; break;
                }
            }
            if (!bAny) { bPass = false; }
        }
        */
        // Date if you want (similar style)

        // Generic Terms: OR logic
        if (bPass && GenericTerms.Num() > 0)
        {
            bool bAnyGeneric = false;
            for (const FString& Term : GenericTerms)
            {
                if (Note->Title.Contains(Term, ESearchCase::IgnoreCase) ||
                    Note->LevelPath.ToString().Contains(Term, ESearchCase::IgnoreCase))
                {
                    bAnyGeneric = true;
                    break;
                }
            }
            if (!bAnyGeneric) { bPass = false; }
        }

        if (bPass)
        {
            FilteredNotes.Add(Note);
        }
    }

    if (NotesListView.IsValid())
    {
        NotesListView->RequestListRefresh();
    }
}

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
        .AutoHeight()
        .HAlign(HAlign_Fill)
        [
            SNew(SEditableTextBox)
            .HintText(FText::FromString("Search (e.g., Map=Test User=Alice Name=\"Big Boss\")"))
            .ToolTipText(FText::FromString(
                "Filtering syntax:\n"
                " field=value     (e.g., Map=Test)\n"
                " field=\"multi word\" (e.g., Name=\"Big Boss\")\n"
                " Use spaces to add more filters (AND)\n"
                " Repeat a field for OR (e.g., Name=Alice Name=Bob)\n"
                " Unqualified terms match any field (OR)\n"
                "Examples:\n"
                " Map=Test Name=Bob\n"
                " Tag=\"Mission Critical\" User=Alice"
            ))
            .OnTextChanged(this, &SDevNoteSelector::OnSearchTextChanged)
        ]
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SAssignNew(NotesListView, SListView<TSharedPtr<FDevNote>>)
            .ListItemsSource(&FilteredNotes)
            .ReturnFocusToSelection(true)
            .OnGenerateRow(this, &SDevNoteSelector::OnGenerateNoteRow)
            .OnSelectionChanged(this, &SDevNoteSelector::OnNoteSelectedInternal)
            .SelectionMode(ESelectionMode::Single)
        ]
    ];
}


void SDevNoteSelector::SetNotesSource(const TArray<TSharedPtr<FDevNote>>& InNotes)
{
    Notes.Empty();
    for (auto& Note : InNotes)
    {
        Notes.Add(Note);
    }

    ParseAndApplyFilters();
}

void SDevNoteSelector::SetSelectedNote(const TSharedPtr<FDevNote>& InNote)
{
    if (NotesListView.IsValid())
    {
        NotesListView->SetSelection(InNote);
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

void SDevNoteSelector::OnNoteSelectedInternal(TSharedPtr<FDevNote> InNote, ESelectInfo::Type SelectionType)
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