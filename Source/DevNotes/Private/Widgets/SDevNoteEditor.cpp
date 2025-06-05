#include "SDevNoteEditor.h"
#include "Widgets/Text/STextBlock.h"

void SDevNoteEditor::Construct(const FArguments& InArgs)
{
    SelectedNote = InArgs._SelectedNote;

    ChildSlot
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
    ];
}

void SDevNoteEditor::SetSelectedNote(TSharedPtr<FDevNote> InNote)
{
    SelectedNote = InNote;
    // Invalidate and force refresh if needed:
    if (this->IsConstructed())
    {
        this->Invalidate(EInvalidateWidget::LayoutAndVolatility);
    }
}
