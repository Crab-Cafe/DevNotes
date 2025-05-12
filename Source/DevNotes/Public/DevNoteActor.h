// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FDevNote.h"
#include "GameFramework/Actor.h"
#include "DevNoteActor.generated.h"

UCLASS(Transient, DisplayName="Dev Note")
class DEVNOTES_API ADevNoteActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ADevNoteActor();

protected:
	
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere)
	FDevNote AssociatedNote;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
