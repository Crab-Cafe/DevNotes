
#pragma once
#include "FDevNoteTag.generated.h"

USTRUCT(BlueprintType)
struct FDevNoteTag
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dev Note")
	FGuid Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dev Note")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dev Note")
	int32 Colour = 0;
};
