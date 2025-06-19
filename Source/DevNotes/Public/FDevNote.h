#pragma once

#include "FDevNote.generated.h"

USTRUCT(BlueprintType)
struct FDevNote
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Dev Note")
	FGuid Id;
	
	UPROPERTY(BlueprintReadOnly, Category="Dev Note")
	FString Title;

	UPROPERTY(BlueprintReadOnly, Category="Dev Note")
	FString Body;

	UPROPERTY(BlueprintReadOnly, Category="Dev Note")
	FGuid CreatedById;

	UPROPERTY(BlueprintReadOnly, Category="Dev Note")
	FDateTime CreatedAt;

	UPROPERTY(BlueprintReadOnly, Category="Dev Note")
	FDateTime LastEdited;
	
	UPROPERTY(BlueprintReadOnly, Category="Dev Note", Category="Dev Note")
	TSoftObjectPtr<UWorld> LevelPath;
	
	UPROPERTY(BlueprintReadOnly, Category="Dev Note")
	FVector WorldPosition;

	UPROPERTY(BlueprintReadOnly, Category="Dev Note")
	TArray<FGuid> Tags;
};
