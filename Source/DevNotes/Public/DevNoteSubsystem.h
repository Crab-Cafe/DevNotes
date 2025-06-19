// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "FDevNote.h"
#include "FDevNoteUser.h"
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
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSignedIn, FString);
DECLARE_MULTICAST_DELEGATE(FOnSignedOut);

UCLASS()
class DEVNOTES_API UDevNoteSubsystem : public UEditorSubsystem 
{
	GENERATED_BODY()

public:
	static UDevNoteSubsystem* Get();
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	const TArray<TSharedPtr<FDevNote>>& GetNotes() const { return CachedNotes; }

	// TODO: Implement diffing for request functions - send the server a list of IDs and Last Edit times, and only download the notes we don't have or the notes that are out of date
	// Fetches all notes from the server. Currently also refreshes users and notes
	void RequestNotesFromServer();

	// Fetches all users from the server
	void RequestUsersFromServer();

	// Fetches all tags from the server
	void RequestTagsFromServer();

	// Create a new note on the server
	UFUNCTION(BlueprintCallable, Category="DevNotes")
	void PostNote(const FDevNote& Note);

	// Update a note in-place on the server
	UFUNCTION(BlueprintCallable, Category="DevNotes")
	void UpdateNote(const FDevNote& Note);

	// Delete a note on the server
	UFUNCTION(BlueprintCallable, Category="DevNotes")
	void DeleteNote(const FGuid& NoteId);

	// Prompts the user to save their current level and teleports their editor camera to the specified note
	UFUNCTION(BlueprintCallable, Category="DevNotes")
	void PromptAndTeleportToNote(const FDevNote& note);
	
	// Erases all note waypoints from the level
	UFUNCTION(BlueprintCallable, Category="DevNotes")
	void ClearAllNoteWaypoints();

	// Erases then reinstantiates all waypoints from the level
	UFUNCTION(BlueprintCallable, Category="DevNotes")
	void RefreshWaypointActors();

	UFUNCTION(BlueprintCallable, Category="DevNotes")
	void PostTag(FDevNoteTag NoteTag);

	UFUNCTION(BlueprintCallable, Category="DevNotes")
	void DeleteTag(const FGuid& TagId);

	static bool ParseUserFromJsonObject(const TSharedPtr<FJsonObject>& JsonObj, FDevNoteUser& OutUser);
	const FDevNoteUser& GetUserById(const FGuid& UserId);

	const TArray<FDevNoteTag>& GetCachedTags() const { return CachedTags; }

	// Callbacks
	FOnNotesUpdated OnNotesUpdated;
	FOnTagsUpdated OnTagsUpdated;
	FOnSignedIn OnSignedIn;
	FOnSignedOut OnSignedOut;

	// JSON Conversion functions - explicit conversions due to needing to format FGuid in a specific way, and other datatype conversions 
	static bool ParseNoteFromJsonObject(const TSharedPtr<FJsonObject>& JsonObj, FDevNote& OutNote);
	static TSharedPtr<FJsonObject> ConvertNoteToJsonObject(const FDevNote& Note);
	static FString SerializeNoteToJsonString(const FDevNote& Note);
	static TSharedPtr<FJsonObject> ConvertTagToJsonObject(const FDevNoteTag& Tag);
	static bool ParseTagFromJsonObject(const TSharedPtr<FJsonObject>& JsonObj, FDevNoteTag& OutTag);
	void ParseAndCacheNotesFromJson(const FString& JsonString);

	// Create a new note + waypoint at the editor camera's location
	void CreateNewNoteAtEditorLocation();

	// Toggle 'editing' state. While editing state is on, notes won't be auto-refreshed. This is to prevent users from having their work erased when editing a note and it refreshes on them
	void SetEditorEditingState(bool bEditing);

	// Send a sign in request to the server
	void SignIn(const FString& UserName, const FString& Password, 
				TFunction<void(bool bSuccess, const FString& Error)> Completion);

	// Send a sign out request to the server
	void SignOut(TFunction<void(bool)> Completion);

	// Are we logged in (do we have a valid session token?)
	bool IsLoggedIn() const;

	// Get the current user data we are signed in as (blank if not signed in)
	const FDevNoteUser& GetCurrentUser();

	bool TryAutoSignIn();
private:
	const FString SessionTokenFileName = TEXT("DevNotes/session.token");
	FTimerHandle RefreshNotesTimerHandle;

	bool bIsEditorEditing = false; // Editing state flag
	bool bRefreshPendingWhileEditing = false; // Wants to refresh once editing flag is toggled off again

	// TODO: Replace with TMap? Looking up by id is a common use case. Also investigate if TSharedPtr is still needed
	TArray<TSharedPtr<FDevNote>> CachedNotes; // Local copy of all notes
	TArray<FDevNoteTag> CachedTags; // Local copy of all tags
	TArray<FDevNoteUser> CachedUsers; // Local copy of all users

	// Current user data
	FGuid CurrentUserId;
	FString SessionToken;

	// Temp storage for actor reselection on waypoint refresh
	UPROPERTY()
	TSet<FGuid> SelectedNoteIDsBeforeRefresh;
	
	// Called every 30 seconds to refresh notes from server
	void OnPollNotesTimerTimeout();

	// Self explanatory
	ADevNoteActor* SpawnWaypointForNote(TSharedPtr<FDevNote> Note);

	// Get a list of waypoints selected in the viewport (only their underlying notes, we dont really care about the actors)
	TArray<TSharedPtr<FDevNote>> GetSelectedNoteWaypoints();
	void StoreSelectedNoteIDs();

	// Http Responses
	void HandleNotesResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);
	void HandleUsersResponse(TSharedPtr<IHttpRequest> HttpRequest, TSharedPtr<IHttpResponse> HttpResponse, bool bWasSuccessful);
	void HandleTagsResponse(TSharedPtr<IHttpRequest> HttpRequest, TSharedPtr<IHttpResponse> HttpResponse, bool bWasSuccessful);

	// All loaded levels and sublevels
	TSet<FString> GetLoadedLevelPaths();

	// Get the desired server connection address from user settings
	FString GetServerAddress() const;

	// Auth functions
	FString GetSessionTokenFilePath() const;
	FString LoadSessionTokenFromFile() const;
	void SaveSessionTokenToFile(const FString& Token) const;
	void DeleteSessionTokenFile() const;
	void SetSessionToken(const FString& Token);
	void ClearSessionToken();
	void SignOutInternal();
	void RetryTokenValidation();
	void HandleTokenInvalidation(TSharedPtr<IHttpResponse> HttpResponse);
};

