#include "SDevNoteSelector.h"

#include "DevNoteSubsystem.h"
#include "FDevNoteTag.h"
#include "StructUtils/PropertyBag.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Text/STextBlock.h"

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
    TMap<FString, TArray<FString>> FieldFilters;
    TArray<FString> GenericTerms;

    TArray<FString> Tokens;
    ParseSearchStringWithQuotes(SearchText.ToString(), Tokens);

    auto tags = UDevNoteSubsystem::Get()->GetCachedTags();
    
    for (const FString& Token : Tokens)
    {
        // Split into key value
        FString Key, Value;
        if (Token.Split(TEXT("="), &Key, &Value))
        {
            Key = Key.ToLower().TrimStartAndEnd();
            Value = Value.TrimQuotes().TrimStartAndEnd();
            FieldFilters.FindOrAdd(Key).Add(Value);
        }
        else
        {
            // Generic (no key - wildcard)
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
        
        // User
        if (FieldFilters.Contains("user"))
        {
            bool bAny = false;
            for (const FString& Val : FieldFilters["user"])
            {
                UDevNoteSubsystem* ss = UDevNoteSubsystem::Get();
                auto user = ss->GetUserById(Note->CreatedById);
                if (ss->GetUserById(Note->CreatedById).Name.Contains(Val, ESearchCase::IgnoreCase))
                {
                    bAny = true; break;
                }
            }
            if (!bAny) { bPass = false; }
        }
        
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
        if (FieldFilters.Contains("tag"))
        {
            bool bAny = false;
            for (const FString& Val : FieldFilters["tag"])
            {
                // Check all tags for this note
                for (const FGuid& NoteTagId : Note->Tags)
                {
                    // Find the tag with this ID
                    if (const FDevNoteTag* FoundTag = tags.FindByPredicate([&NoteTagId](const FDevNoteTag& NoteTag) { 
                        return NoteTag.Id == NoteTagId; 
                    }))
                    {
                        if (FoundTag->Name.Contains(Val, ESearchCase::IgnoreCase))
                        {
                            bAny = true;
                            break;
                        }
                    }
                }
                if (bAny) break; // Exit outer loop if we found a match
            }
            if (!bAny) { bPass = false; }
        }

        

        // Generic/wildcard terms
        if (bPass && GenericTerms.Num() > 0)
        {
            bool bAnyGeneric = false;
            for (const FString& Term : GenericTerms)
            {
                // Title, level, user
                if (Note->Title.Contains(Term, ESearchCase::IgnoreCase) ||
                    Note->LevelPath.ToString().Contains(Term, ESearchCase::IgnoreCase) ||
                    UDevNoteSubsystem::Get()->GetUserById(Note->CreatedById).Name.Contains(Term, ESearchCase::IgnoreCase))
                {
                    bAnyGeneric = true;
                    break;
                }
                
                // Check all tags for this note
                for (const FGuid& NoteTagId : Note->Tags)
                {
                    if (const FDevNoteTag* FoundTag = tags.FindByPredicate([&NoteTagId](const FDevNoteTag& NoteTag) { 
                        return NoteTag.Id == NoteTagId; 
                    }))
                    {
                        if (FoundTag->Name.Contains(Term, ESearchCase::IgnoreCase))
                        {
                            bAnyGeneric = true;
                            break;
                        }
                    }
                }
                if (bAnyGeneric) break; // Exit outer loop if we found a match
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
        // Tool buttons
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
        
        //Search / filter bar
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

        // Note editor
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
    // Get cached tags from subsystem
    auto CachedTags = UDevNoteSubsystem::Get()->GetCachedTags();
    
    TSharedPtr<SHorizontalBox> TagCirclesBox = SNew(SHorizontalBox);
    
    // Tag circle for each tag on note
    for (const FGuid& TagId : InNote->Tags)
    {
        if (const FDevNoteTag* FoundTag = CachedTags.FindByPredicate([&TagId](const FDevNoteTag& NoteTag) { 
            return NoteTag.Id == TagId; 
        }))
        {
            FColor TagColor;
            TagColor.DWColor() = FoundTag->Colour;
            FLinearColor DisplayColor = FLinearColor::FromSRGBColor(TagColor);
            
            TagCirclesBox->AddSlot()
            .AutoWidth()
            .Padding(2.0f, 0.0f)
            [
                SNew(SBox)
                .WidthOverride(12.0f)
                .HeightOverride(12.0f)
                .ToolTipText(FText::FromString(FString::Printf(TEXT("%s\nColor: R=%.2f G=%.2f B=%.2f"), 
                    *FoundTag->Name, 
                    DisplayColor.R, 
                    DisplayColor.G, 
                    DisplayColor.B)))
                [
                    SNew(SImage)
                    .Image(FAppStyle::GetBrush("Icons.FilledCircle"))
                    .ColorAndOpacity(DisplayColor)
                ]
            ];
        }
    }
    
    return SNew(STableRow<TSharedPtr<FDevNote>>, OwnerTable)
    [
        SNew(SHorizontalBox)
        
        // Note title
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Text(FText::FromString(InNote->Title))
        ]
        
        // Tag circles
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(8.0f, 0.0f, 0.0f, 0.0f)
        [
            TagCirclesBox.ToSharedRef()
        ]
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