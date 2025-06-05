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
	// Sets default values for this actor's properties
	ADevNoteActor();

protected:
	
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "DevNote")
	FGuid NoteId;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
