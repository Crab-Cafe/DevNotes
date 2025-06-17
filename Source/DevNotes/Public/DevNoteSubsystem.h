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

struct FDevNoteTag;
class ADevNoteActor;
DECLARE_MULTICAST_DELEGATE(FOnNotesUpdated);
DECLARE_MULTICAST_DELEGATE(FOnTagsUpdated);

UCLASS()
class DEVNOTES_API UDevNoteSubsystem : public UEditorSubsystem, public FTickableEditorObject
{
	GENERATED_BODY()

public:
	// TickableEditorObject interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override { return true; }
	
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UDevNoteSubsystem, STATGROUP_Tickables); }
	void OnPollNotesTimer();
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	void RequestNotesFromServer();
	
	const TArray<TSharedPtr<FDevNote>>& GetNotes() const { return CachedNotes; }
	
	UFUNCTION(BlueprintCallable, Category="DevNotes")
	void PostNote(const FDevNote& Note);
	void UpdateNote(const FDevNote& Note);
	void DeleteNote(const FGuid& NoteId);

	void PromptAndTeleportToNote(const FDevNote& note);
	
	ADevNoteActor* SpawnWaypointForNote(TSharedPtr<FDevNote> Note);
	void ClearAllNoteWaypoints();
	void RefreshWaypointActors();
	TArray<TSharedPtr<FDevNote>> GetSelectedNotes();
	void StoreSelectedNoteIDs();
	void PostTag(FDevNoteTag NoteTag);

	FOnNotesUpdated OnNotesUpdated;
	FOnTagsUpdated OnTagsUpdated;
	FTimerHandle RefreshNotesTimerHandle;

	static bool ParseNoteFromJsonObject(const TSharedPtr<FJsonObject>& JsonObj, FDevNote& OutNote);
	static TSharedPtr<FJsonObject> ConvertNoteToJsonObject(const FDevNote& Note);
	static FString SerializeNoteToJsonString(const FDevNote& Note);

	static TSharedPtr<FJsonObject> ConvertTagToJsonObject(const FDevNoteTag& Tag);
	static bool ParseTagFromJsonObject(const TSharedPtr<FJsonObject>& JsonObj, FDevNoteTag& OutTag);

	
	void CreateNewNoteAtEditorLocation();
	void SetEditorEditingState(bool bEditing);

	void HandleTagsResponse(TSharedPtr<IHttpRequest> HttpRequest, TSharedPtr<IHttpResponse> HttpResponse, bool bArg);
	void RequestTagsFromServer();
	const TArray<FDevNoteTag>& GetCachedTags() const { return CachedTags; }


	static UDevNoteSubsystem* Get();
	bool TryAutoSignIn();

private:
	bool bIsEditorEditing = false;
	bool bRefreshPendingWhileEditing = false;

	TArray<TSharedPtr<FDevNote>> CachedNotes;
	TArray<FDevNoteTag> CachedTags;

	FString SessionToken;
	
	UPROPERTY()
	TSet<FGuid> SelectedNoteIDsBeforeRefresh;
	
	void ParseNotesFromJson(const FString& JsonString);
	void HandleNotesResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	TSet<FString> GetLoadedLevelPaths();
	FString GetServerAddress() const;

	const FString SessionTokenFileName = TEXT("DevNotes/session.token");
	FString GetSessionTokenFilePath() const;
	FString LoadSessionToken() const;
	void SaveSessionToken(const FString& Token) const;
	void DeleteSessionToken() const;

public:
	void SignIn(const FString& UserName, const FString& Password, 
	            TFunction<void(bool bSuccess, const FString& Error)> Completion);


	void SignOut(TFunction<void(bool)> Completion);
};

