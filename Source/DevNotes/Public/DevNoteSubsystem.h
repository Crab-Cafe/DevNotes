// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FDevNote.h"
#include "Subsystems/EngineSubsystem.h"
#include "DevNoteSubsystem.generated.h"

/**
 * 
 */
UCLASS()
class DEVNOTES_API UDevNoteSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	void FetchNotes();
	void PushNote(const FDevNote& note);
	

	UPROPERTY()
	TArray<FDevNote> AllNotes;
};
