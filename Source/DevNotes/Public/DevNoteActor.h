// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FDevNote.h"
#include "GameFramework/Actor.h"
#include "DevNoteActor.generated.h"

UCLASS(Transient, DisplayName="Dev Note", NotPlaceable)
class DEVNOTES_API ADevNoteActor : public AActor
{
	friend class UDevNoteSubsystem;
	GENERATED_BODY()

public:
	ADevNoteActor();

	// This this the CDO or currently open in the blueprint editor?
	bool IsCDO();

	// Get a copy of the note represented by this actor. Exists because BP cannot use a TSharedPtr
	UFUNCTION(BlueprintPure, BlueprintCallable)
	const FDevNote GetNote() const;

	// The note this actor represents
	TSharedPtr<FDevNote> Note;

	// Flag to prevent property updates before actor is fully constructed
	bool bReadyForSync = false;
protected:
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;
};
