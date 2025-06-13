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
	bool IsCDO();

	UFUNCTION(BlueprintPure, BlueprintCallable)
	const FDevNote GetNote() const;

	TSharedPtr<FDevNote> Note;
	bool bReadyForSync = false;
protected:
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;
};
