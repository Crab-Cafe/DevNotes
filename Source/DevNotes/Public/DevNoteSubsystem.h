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
class DEVNOTES_API UDevNoteSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	void RequestNotesFromServer();
	const TArray<FDevNote>& GetNotes() const { return CachedNotes; }
	
	UFUNCTION(BlueprintCallable, Category="DevNotes")
	void PostNote(const FDevNote& Note);

	FOnNotesUpdated OnNotesUpdated;

	static bool ParseNoteFromJsonObject(const TSharedPtr<FJsonObject>& JsonObj, FDevNote& OutNote);
	static TSharedPtr<FJsonObject> ConvertNoteToJsonObject(const FDevNote& Note);
	static FString SerializeNoteToJsonString(const FDevNote& Note);

	void CreateNewNoteAtEditorLocation();
private:
	UPROPERTY()
	TArray<FDevNote> CachedNotes;
	
	void ParseNotesFromJson(const FString& JsonString);
	void HandleNotesResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	
	FString GetServerAddress() const;
};
