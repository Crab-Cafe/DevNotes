#pragma once

#include "FDevNoteTag.h"
#include "FDevNote.generated.h"

USTRUCT()
struct DEVNOTES_API FDevNote
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere)
	FName Id;

	UPROPERTY(VisibleAnywhere, meta=(RowType="/Script/DevNotes.DevNoteUser"))
	FDataTableRowHandle CreatedBy;

	UPROPERTY(VisibleAnywhere)
	FString Title;

	UPROPERTY(VisibleAnywhere)
	FString Body;

	UPROPERTY(VisibleAnywhere, meta=(RowType="/Script/DevNotes.DevNoteTag"))
	TArray<FDataTableRowHandle> Tags;

	UPROPERTY(VisibleAnywhere)
	TMap<FName, FString> Metadata;
};