#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "FDevNote.h"

struct FDevNoteTag;
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

	void OnTagSelectionChanged(const TArray<FGuid>& Guids);
	void OnNewTagCreated(const FDevNoteTag& DevNoteTag);


private:
	TSharedPtr<FDevNote> SelectedNote;
	FString TitleText;
	FString BodyText;
	
	TArray<TSharedPtr<FDevNoteTag>> TagsList;
};
