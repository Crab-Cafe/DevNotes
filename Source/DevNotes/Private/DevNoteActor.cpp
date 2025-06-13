// Fill out your copyright notice in the Description page of Project Settings.


#include "DevNoteActor.h"
#include "DevNoteSubsystem.h"


// Sets default values
ADevNoteActor::ADevNoteActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

#if WITH_EDITORONLY_DATA
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	AActor::SetActorHiddenInGame(true);
	bIsEditorOnlyActor = true;
	SetActorEnableCollision(false);
	bListedInSceneOutliner = false;
#endif
}



void ADevNoteActor::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	if (IsCDO()) return;
	if (!Note) return;
	if (!bReadyForSync) return;
	
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
	// Is CDO?
	if (HasAnyFlags(RF_ClassDefaultObject) || HasAnyFlags(RF_ArchetypeObject))
	{
		return true;
	}

	return false;
}

const FDevNote ADevNoteActor::GetNote() const
{
	if (Note)
		return *Note;

	return FDevNote();
}


