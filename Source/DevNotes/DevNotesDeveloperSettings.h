// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "DevNotesDeveloperSettings.generated.h"

/**
 * 
 */
UCLASS(Config="DevNotes")
class DEVNOTES_API UDevNotesDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditDefaultsOnly)
	FString ServerAddress;
	
	UPROPERTY(Config, EditDefaultsOnly, meta=(RequiredAssetDataTags = "RowStructure=/Script/DevNotes.DevNoteUser"))
	TSoftObjectPtr<UDataTable> UsersDatatable;

	UPROPERTY(Config, EditDefaultsOnly, meta=(RequiredAssetDataTags = "RowStructure=/Script/DevNotes.DevNoteTag"))
	TSoftObjectPtr<UDataTable> TagsDatatable;
};
