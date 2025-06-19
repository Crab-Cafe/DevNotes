// Fill out your copyright notice in the Description page of Project Settings.


#include "DevNoteActor.h"
#include "DevNoteSubsystem.h"


// Sets default values
ADevNoteActor::ADevNoteActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	
	AActor::SetActorHiddenInGame(true);
	bIsEditorOnlyActor = true;
	SetActorEnableCollision(false);
	bListedInSceneOutliner = false;
}



void ADevNoteActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	if (IsCDO()) return;
	if (!Note) return;
	if (!bReadyForSync) return;

	// When a property is changed, push an update to the server
	if (UDevNoteSubsystem* Subsystem = GEditor->GetEditorSubsystem<UDevNoteSubsystem>())
	{
		Note->WorldPosition = GetActorLocation();
		Subsystem->UpdateNote(*Note);
	}
	
}

void ADevNoteActor::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);
	if (IsCDO()) return;
	if (!Note) return;
	if (!bReadyForSync) return;

	// When a note waypoint is moved, push an update to the server
	if (bFinished)
	{
		if (UDevNoteSubsystem* Subsystem = GEditor->GetEditorSubsystem<UDevNoteSubsystem>())
		{
			Note->WorldPosition = GetActorLocation();
			Subsystem->UpdateNote(*Note);
		}
	}
}

bool ADevNoteActor::IsCDO()
{
	return (HasAnyFlags(RF_ClassDefaultObject) || HasAnyFlags(RF_ArchetypeObject));
}

const FDevNote ADevNoteActor::GetNote() const
{
	if (Note)
		return *Note;

	return FDevNote();
}


