#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "FDevNote.h"

class UDevNoteSubsystem;

class SDevNoteEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDevNoteEditor) {}
	SLATE_ARGUMENT(TSharedPtr<FDevNote>, SelectedNote)
	SLATE_END_ARGS()

	FString GetLevelPath() const;
	void OnLevelPathChanged(const FAssetData& AssetData);
	void Construct(const FArguments& InArgs);
	void SetSelectedNote(TSharedPtr<FDevNote> InNote);

private:
	TSharedPtr<FDevNote> SelectedNote;
	FString TitleText;
	FString BodyText;
};
