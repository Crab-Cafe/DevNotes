// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DevNoteActor.h"
#include "Engine/DeveloperSettings.h"
#include "DevNotesDeveloperSettings.generated.h"

/**
 * 
 */
UCLASS(Config="DevNotes", DefaultConfig)
class DEVNOTES_API UDevNotesDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditDefaultsOnly)
	FString ServerAddress;

	UPROPERTY(Config, EditDefaultsOnly)
	TSoftClassPtr<ADevNoteActor> DevNoteActorRepresentation;
};
