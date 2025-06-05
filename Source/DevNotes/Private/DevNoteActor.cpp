// Fill out your copyright notice in the Description page of Project Settings.


#include "DevNoteActor.h"


// Sets default values
ADevNoteActor::ADevNoteActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

#if WITH_EDITORONLY_DATA
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	AActor::SetActorHiddenInGame(true);
	bIsEditorOnlyActor = true;
#endif
}

// Called when the game starts or when spawned
void ADevNoteActor::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ADevNoteActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

