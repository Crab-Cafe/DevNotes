#pragma once

#include "FDevNote.generated.h"

USTRUCT(BlueprintType)
struct FDevNote
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FGuid Id;
	
	UPROPERTY(BlueprintReadOnly)
	FString Title;

	UPROPERTY(BlueprintReadOnly)
	FString Body;

	UPROPERTY(BlueprintReadOnly)
	FGuid CreatedById;

	UPROPERTY(BlueprintReadOnly)
	FDateTime CreatedAt;

	UPROPERTY(BlueprintReadOnly)
	FDateTime LastEdited;
	
	UPROPERTY(BlueprintReadOnly)
	TSoftObjectPtr<UWorld> LevelPath;
	
	UPROPERTY(BlueprintReadOnly)
	FVector WorldPosition;
};
