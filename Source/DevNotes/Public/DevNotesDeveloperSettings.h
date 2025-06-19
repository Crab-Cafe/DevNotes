// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DevNoteActor.h"
#include "Engine/DeveloperSettings.h"
#include "DevNotesDeveloperSettings.generated.h"

/**
 * 
 */
UCLASS(Config="DevNotes", DefaultConfig, meta=(DisplayName="DevNotes Settings"), Category="Plugins")
class DEVNOTES_API UDevNotesDeveloperSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// Address of the DevNotes backend server
	UPROPERTY(Config, EditDefaultsOnly, Category="Dev Note")
	FString ServerAddress = "http://localhost/7124";

	// Actor used to represent a note in the world
	UPROPERTY(Config, EditDefaultsOnly, Category="Dev Note")
	TSoftClassPtr<ADevNoteActor> DevNoteActorRepresentation = ADevNoteActor::StaticClass();
};
