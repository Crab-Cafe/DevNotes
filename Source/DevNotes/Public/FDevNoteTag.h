
#pragma once
#include "FDevNoteTag.generated.h"

USTRUCT()
struct DEVNOTES_API FDevNoteTag : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY()
	FString Name;

	UPROPERTY()
	FColor Color;
};