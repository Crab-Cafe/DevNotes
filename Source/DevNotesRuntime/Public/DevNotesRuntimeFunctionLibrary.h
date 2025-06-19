// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DevNotesRuntimeFunctionLibrary.generated.h"

/**
 * 
 */
UCLASS()
class DEVNOTESRUNTIME_API UDevNotesRuntimeFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	UFUNCTION(BlueprintCallable, Category = "Dev Notes", meta = (AutoCreateRefTerm="Tags"))
	static bool UploadNote(FString Server, FString AuthKey, FString AsUser, FString Title,
					FString Body, FVector Location, TSoftObjectPtr<UWorld> Level, const TArray<FString>& Tags);

	UFUNCTION(BlueprintPure, Category = "Dev Notes", meta = (DefaultToSelf="Actor"))
	static TSoftObjectPtr<UWorld> GetActorLevelAsset(AActor* Actor);
};
