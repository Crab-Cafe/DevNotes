#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "FDevNote.h"
#include "SDevNoteTagPicker.h"

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
	void OnTagPickerOpened();
	void Construct(const FArguments& InArgs);
	void SetSelectedNote(TSharedPtr<FDevNote> InNote);

private:
	TSharedPtr<FDevNote> SelectedNote;
	FString TitleText;
	FString BodyText;
	
	TArray<TSharedPtr<FDevNoteTag>> TagsList;
	TSharedPtr<SDevNoteTagPicker> TagPicker;
    
	// Tag-related methods
	void RefreshTagsList();
	void OnTagSelectionChanged(const TArray<FGuid>& NewTagIds);
	void OnNewTagCreated(const FDevNoteTag& NewTag);

	void OnTagAdded(FGuid TagId) const;
	void OnTagRemoved(FGuid TagId) const;

	
	TSharedRef<SWidget> CreateTagDisplay() const;
	TSharedPtr<SBox> TagDisplayWidget;


};
