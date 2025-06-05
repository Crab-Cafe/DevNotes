// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FDevNote.h"
#include "HttpFwd.h"
#include "Subsystems/EngineSubsystem.h"
#include "DevNoteSubsystem.generated.h"

/**
 * 
 */

DECLARE_MULTICAST_DELEGATE(FOnNotesUpdated);

UCLASS()
class DEVNOTES_API UDevNoteSubsystem : public UEditorSubsystem, public FTickableEditorObject
{
	GENERATED_BODY()

public:
	// TickableEditorObject interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return true; }
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UDevNoteSubsystem, STATGROUP_Tickables); }
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	void RequestNotesFromServer();
	
	const TArray<TSharedPtr<FDevNote>>& GetNotes() const { return CachedNotes; }
	
	UFUNCTION(BlueprintCallable, Category="DevNotes")
	void PostNote(const FDevNote& Note);
	void UpdateNote(const FDevNote& Note);
	void DeleteNote(const FGuid& NoteId);

	void PromptAndTeleportToNote(const FDevNote& note);
	
	void SpawnWaypointForNote(TSharedPtr<FDevNote> Note);
	void ClearAllNoteWaypoints();
	void RefreshWaypointActors();

	FOnNotesUpdated OnNotesUpdated;

	TSet<FString> GetLoadedLevelPaths();

	static bool ParseNoteFromJsonObject(const TSharedPtr<FJsonObject>& JsonObj, FDevNote& OutNote);
	static TSharedPtr<FJsonObject> ConvertNoteToJsonObject(const FDevNote& Note);
	static FString SerializeNoteToJsonString(const FDevNote& Note);
	
	void CreateNewNoteAtEditorLocation();
private:
	TArray<TSharedPtr<FDevNote>> CachedNotes;
	
	void ParseNotesFromJson(const FString& JsonString);
	void HandleNotesResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	
	FString GetServerAddress() const;
};

