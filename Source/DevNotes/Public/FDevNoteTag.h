
#pragma once
#include "FDevNoteTag.generated.h"

USTRUCT(BlueprintType)
struct FDevNoteTag
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGuid Id;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Colour = 0;
};
