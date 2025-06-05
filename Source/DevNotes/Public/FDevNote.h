#pragma once

#include "FDevNote.generated.h"

USTRUCT(BlueprintType)
struct FDevNote
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid Id;
	
	UPROPERTY()
	FString Title;

	UPROPERTY()
	FString Body;

	UPROPERTY()
	FGuid CreatedById;

	UPROPERTY()
	FDateTime CreatedAt;
	
	UPROPERTY()
	TSoftObjectPtr<UWorld> LevelPath;
	
	UPROPERTY()
	FVector WorldPosition;
};
