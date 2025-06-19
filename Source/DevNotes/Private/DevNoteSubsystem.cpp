// Fill out your copyright notice in the Description page of Project Settings.


#include "DevNoteSubsystem.h"

#include "DevNotesLog.h"
#include "EngineUtils.h"
#include "FDevNoteTag.h"
#include "FileHelpers.h"
#include "HttpModule.h"
#include "HttpServerConstants.h"
#include "JsonObjectConverter.h"
#include "LevelEditorSubsystem.h"
#include "LevelEditorViewport.h"
#include "Selection.h"
#include "DevNotesDeveloperSettings.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"


#if WITH_EDITOR
#include "Editor.h"
#include "EditorLevelLibrary.h"
#endif


FString GetCurrentLevelPath()
{
#if WITH_EDITOR
	// Get the current editor world
	UWorld* EditorWorld = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (EditorWorld)
	{
		// Get the current level package name
		return EditorWorld->GetCurrentLevel()->GetOutermost()->GetName();
	}
#endif
	return FString();
}


FVector GetEditorViewportCameraLocation()
{
#if WITH_EDITOR
	// Get Level Editor viewport client
	for (FEditorViewportClient* ViewportClient : GEditor->GetAllViewportClients())
	{
		// Filter for perspective viewport and Level Editor type
		if (ViewportClient && ViewportClient->IsPerspective() && ViewportClient->IsVisible())
		{
			return ViewportClient->GetViewLocation();
		}
	}
#endif
	return FVector::ZeroVector;
}


void UDevNoteSubsystem::OnPollNotesTimerTimeout()
{
	// Only run if we are logged in
	// TODO: Stop the timer if logged out, or try to establish a connection
	if (!IsLoggedIn()) return;
		
	if (!bIsEditorEditing)
	{
		RequestNotesFromServer();
	}
	else
	{
		bRefreshPendingWhileEditing = true;
	}

}

bool UDevNoteSubsystem::TryAutoSignIn()
{
	const FString SavedToken = LoadSessionTokenFromFile();
	if (SavedToken.IsEmpty())
	{
		UE_LOG(LogDevNotes, Log, TEXT("No saved session token found"));
		return false;
	}

	// Set the token optimistically. If the server rejects it, it will be cleared upon repsonse
	SessionToken = SavedToken;
	UE_LOG(LogDevNotes, Log, TEXT("Loaded session token from file, validating with server..."));

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(GetServerAddress() + TEXT("/validatetoken"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	JsonObject->SetStringField(TEXT("token"), SavedToken);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	
	Request->SetContentAsString(OutputString);

	Request->OnProcessRequestComplete().BindLambda(
		[this, SavedToken](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bWasSuccessful)
		{
			if (bWasSuccessful && HttpResponse.IsValid())
			{
				const int32 ResponseCode = HttpResponse->GetResponseCode();
				UE_LOG(LogDevNotes, Log, TEXT("Token validation response: %d"), ResponseCode);
				
				if (ResponseCode == EHttpResponseCodes::Ok)
				{
					// Token is valid - we're already set up, just broadcast the event
					UE_LOG(LogDevNotes, Log, TEXT("Session token validated successfully"));
					TSharedPtr<FJsonObject> JsonObj;
					const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(HttpResponse->GetContentAsString());

					if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
					{
						FString UserIdString;
						if (JsonObj->TryGetStringField(TEXT("Id"), UserIdString))
						{
							CurrentUserId = FGuid(UserIdString);
							UE_LOG(LogDevNotes, Log, TEXT("Current user ID restored: %s"), *CurrentUserId.ToString());
						}
					}
					OnSignedIn.Broadcast(SavedToken);
				}
				else if (ResponseCode == EHttpResponseCodes::Denied || ResponseCode == EHttpResponseCodes::Forbidden)
				{
					// Token is explicitly rejected by server - clear it
					UE_LOG(LogDevNotes, Warning, TEXT("Session token rejected by server (code: %d) - clearing"), ResponseCode);
					ClearSessionToken();
					OnSignedOut.Broadcast();
				}
				else
				{
					// Other server error - keep token but don't sign in yet
					UE_LOG(LogDevNotes, Warning, TEXT("Server error during token validation (code: %d) - keeping token for retry"), ResponseCode);
				}
			}
			else
			{
				// Network error or no response - keep the token, don't sign in yet
				// Covers the case when the server is temporarily unavailable, for instance due to connection issues
				UE_LOG(LogDevNotes, Warning, TEXT("Failed to validate token due to network error - keeping token for retry"));
			}
		});

	Request->ProcessRequest();
	return true;
}

const FDevNoteUser& UDevNoteSubsystem::GetCurrentUser()
{
	// Find the current user in our cached users list
	if (CurrentUserId.IsValid())
	{
		if (const FDevNoteUser* FoundUser = CachedUsers.FindByPredicate([this](const FDevNoteUser& User) {
			return User.Id == CurrentUserId;
		}))
		{
			return *FoundUser;
		}
	}
	
	// Return a static empty user if not found or not logged in
	static FDevNoteUser EmptyUser;
	return EmptyUser;
}


void UDevNoteSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	// Try to restore session from saved token
	TryAutoSignIn();
	
	OnSignedIn.AddWeakLambda(this, [this](FString Token)
	{
		UE_LOG(LogDevNotes, Log, TEXT("Signed in successfully, starting data sync..."));
		RequestTagsFromServer();
		RequestNotesFromServer();
		
		// Start polling timer
		GEditor->GetTimerManager()->SetTimer(
			RefreshNotesTimerHandle,
			this,
			&UDevNoteSubsystem::OnPollNotesTimerTimeout,
			30.0f,
			true);
	});
}

void UDevNoteSubsystem::RequestNotesFromServer()
{
	RequestTagsFromServer();
	RequestUsersFromServer();
	FHttpModule* Http = &FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
	
	Request->OnProcessRequestComplete().BindUObject(this, &UDevNoteSubsystem::HandleNotesResponse);
	Request->SetURL(GetServerAddress() + TEXT("/notes"));
	Request->SetVerb("GET");
	Request->SetHeader("Content-Type", "application/json");
	Request->SetHeader(TEXT("X-Session-Token"), *SessionToken);
	Request->ProcessRequest();
}

void UDevNoteSubsystem::PostNote(const FDevNote& Note)
{
	FString JsonString = SerializeNoteToJsonString(Note);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(GetServerAddress() + "/notes");
	Request->SetVerb("POST");
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("X-Session-Token"), *SessionToken);
	Request->SetContentAsString(JsonString);

	Request->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr Req, FHttpResponsePtr Response, bool bSuccess)
	{
		HandleTokenInvalidation(Response);

		if (bSuccess && Response->GetResponseCode() == EHttpResponseCodes::Created)
		{
			UE_LOG(LogDevNotes, Log, TEXT("Note posted successfully."));
			RequestNotesFromServer();
		}
		else
		{
			if (Response)
			{
				UE_LOG(LogDevNotes, Error, TEXT("Failed to post note: %s. \nError: %d \nReason:%hhd"), *Response->GetContentAsString(), Response->GetResponseCode(), Response->GetFailureReason());
			}
			else
			{
				UE_LOG(LogDevNotes, Error, TEXT("Could not get a response from server: %s"), *Req->GetURL());
			}
		}
	});

	Request->ProcessRequest();
}

void UDevNoteSubsystem::UpdateNote(const FDevNote& Note)
{
	FString JsonString = SerializeNoteToJsonString(Note);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(GetServerAddress() + "/notes/" + Note.Id.ToString(EGuidFormats::DigitsWithHyphens));
	Request->SetVerb("PUT");
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("X-Session-Token"), *SessionToken);
	Request->SetContentAsString(JsonString);

	Request->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr Req, FHttpResponsePtr Response, bool bSuccess)
	{
		HandleTokenInvalidation(Response);

		if (bSuccess && Response->GetResponseCode() == EHttpResponseCodes::Ok)
		{
			UE_LOG(LogDevNotes, Log, TEXT("Note updated successfully."));
			RequestNotesFromServer();
		}
		else
		{
			if (Response)
			{
				UE_LOG(LogDevNotes, Error, TEXT("Failed to update note: %s. \nError: %d \nReason:%hhd"), *Response->GetContentAsString(), Response->GetResponseCode(), Response->GetFailureReason());
			}
			else
			{
				UE_LOG(LogDevNotes, Error, TEXT("Could not get a response from server: %s"), *Req->GetURL());
			}
		}
	});

	Request->ProcessRequest();
}

void UDevNoteSubsystem::DeleteNote(const FGuid& NoteId)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(GetServerAddress() + "/notes/" + NoteId.ToString(EGuidFormats::DigitsWithHyphens));
	Request->SetVerb("DELETE");
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("X-Session-Token"), *SessionToken);

	Request->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr Req, FHttpResponsePtr Response, bool bSuccess)
	{
		HandleTokenInvalidation(Response);

		if (bSuccess && Response->GetResponseCode() == EHttpResponseCodes::NoContent)
		{
			UE_LOG(LogDevNotes, Log, TEXT("Note deleted successfully."));
			RequestNotesFromServer();
		}
		else
		{
			if (Response)
			{
				UE_LOG(LogDevNotes, Error, TEXT("Failed to delete note: %s"), *Response->GetContentAsString());
			}
			else
			{
				UE_LOG(LogDevNotes, Error, TEXT("Could not get a response from server: %s"), *Req->GetURL());
			}
		}
	});

	Request->ProcessRequest();
}

void UDevNoteSubsystem::PromptAndTeleportToNote(const FDevNote& note)
{
	auto TargetLevelPath = note.LevelPath.GetLongPackageName();
	bool success = true;
	if (TargetLevelPath != GetCurrentLevelPath())
	{
		// We don't fuck with /Temp/ levels. Save them first.
		bool validLevelPath = !TargetLevelPath.IsEmpty() && !TargetLevelPath.StartsWith("/Temp/");
		if (!validLevelPath)
		{
				success = false;
		}
		else
		{
			// Open confirmation dialogue
			EAppReturnType::Type Choice = FMessageDialog::Open(
			EAppMsgType::YesNoCancel,
			FText::FromString(TEXT("Teleporting to another level: " + TargetLevelPath + "\nSave current level before continuing?"))
			);

			switch (Choice)
			{
			case EAppReturnType::Yes:
				{
					UEditorLoadingAndSavingUtils::SaveDirtyPackages(true, false);
				}
				// falls through intentionally
			case EAppReturnType::No:
				{
					// Open the target level
					ULevelEditorSubsystem* levelEditorSS = GEditor->GetEditorSubsystem<ULevelEditorSubsystem>();
					success = levelEditorSS->LoadLevel(TargetLevelPath);
					break;
				}
			case EAppReturnType::Cancel:
				return;
			default:
				break;
			}
		}
	}

	if (!success)
	{
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(TEXT("Failed to open level: ") + TargetLevelPath));
		return;
	}

	// Teleport editor camera to note's location
	for (FLevelEditorViewportClient* ViewportClient : GEditor->GetLevelViewportClients())
	{
		if (ViewportClient && ViewportClient->IsPerspective())
		{
			ViewportClient->SetViewLocation(note.WorldPosition);
			ViewportClient->Invalidate();
		}
	}
}

TSet<FString> UDevNoteSubsystem::GetLoadedLevelPaths()
{
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return {};

	TSet<FString> LoadedLevelPaths;

	// Retrieve all sublevels - untested
	for (ULevel* Level : World->GetLevels())
	{
		if (!Level || !Level->GetOutermost()) continue;

		FString Path = Level->GetOutermost()->GetName();
		LoadedLevelPaths.Add(Path);
	}

	return LoadedLevelPaths;
}

bool UDevNoteSubsystem::ParseNoteFromJsonObject(const TSharedPtr<FJsonObject>& JsonObj, FDevNote& OutNote)
{
	if (!JsonObj.IsValid())
	{
		return false;
	}

	FString idString;
	FString createdByIDString;

	if (!JsonObj->TryGetStringField(TEXT("id"), idString) ||
		!JsonObj->TryGetStringField(TEXT("title"), OutNote.Title) ||
		!JsonObj->TryGetStringField(TEXT("body"), OutNote.Body) ||
		!JsonObj->TryGetStringField(TEXT("createdById"), createdByIDString))
	{
		return false; // Required fields missing
	}

	OutNote.Id = FGuid(idString);
	OutNote.CreatedById = FGuid(createdByIDString);
	
	double X = 0.0, Y = 0.0, Z = 0.0;
	JsonObj->TryGetNumberField(TEXT("worldX"), X);
	JsonObj->TryGetNumberField(TEXT("worldY"), Y);
	JsonObj->TryGetNumberField(TEXT("worldZ"), Z);
	OutNote.WorldPosition = FVector(X, Y, Z);

	FString outPath;
	JsonObj->TryGetStringField(TEXT("levelPath"), outPath);
	OutNote.LevelPath = FSoftObjectPath(outPath);

	FString dateTimeString;
	JsonObj->TryGetStringField(TEXT("createdAt"), dateTimeString);
	FDateTime::ParseIso8601(*dateTimeString, OutNote.CreatedAt);

	FString lastEditedString;
	JsonObj->TryGetStringField(TEXT("lastEdited"), lastEditedString);
	FDateTime::ParseIso8601(*lastEditedString, OutNote.LastEdited);

	OutNote.Tags.Empty();
	const TArray<TSharedPtr<FJsonValue>>* TagIdsArray;
	if (JsonObj->TryGetArrayField(TEXT("tags"), TagIdsArray))
	{
		for (const TSharedPtr<FJsonValue>& Val : *TagIdsArray)
		{
			FString TagIdStr = Val->AsObject()->GetStringField(TEXT("id"));
			FGuid Guid;
			if (FGuid::Parse(TagIdStr, Guid))
			{
				OutNote.Tags.Add(Guid);
			}
		}
	}

	return true;
}

TSharedPtr<FJsonObject> UDevNoteSubsystem::ConvertNoteToJsonObject(const FDevNote& Note)
{
	TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();

	JsonObject->SetStringField(TEXT("id"), Note.Id.ToString(EGuidFormats::DigitsWithHyphens));
	JsonObject->SetStringField(TEXT("title"), Note.Title);
	JsonObject->SetStringField(TEXT("body"), Note.Body);
	JsonObject->SetStringField(TEXT("createdById"), Note.CreatedById.ToString(EGuidFormats::DigitsWithHyphens));

	JsonObject->SetNumberField(TEXT("worldX"), Note.WorldPosition.X);
	JsonObject->SetNumberField(TEXT("worldY"), Note.WorldPosition.Y);
	JsonObject->SetNumberField(TEXT("worldZ"), Note.WorldPosition.Z);

	JsonObject->SetStringField(TEXT("levelPath"), Note.LevelPath.ToString());
	JsonObject->SetStringField(TEXT("createdAt"), Note.CreatedAt.ToIso8601());
	JsonObject->SetStringField(TEXT("lastEdited"), FDateTime::UtcNow().ToIso8601());

	TArray<TSharedPtr<FJsonValue>> TagIdsArray;
	for (const FGuid& TagId : Note.Tags)
	{
		TSharedPtr<FJsonObject> tagObj = MakeShared<FJsonObject>();
		tagObj->SetStringField(TEXT("id"), TagId.ToString(EGuidFormats::DigitsWithHyphens));

		TSharedPtr<FJsonValueObject> val = MakeShared<FJsonValueObject>(tagObj);
		TagIdsArray.Add(val);
	}
	
	JsonObject->SetArrayField(TEXT("tags"), TagIdsArray);
	
	return JsonObject;
}

TSharedPtr<FJsonObject> UDevNoteSubsystem::ConvertTagToJsonObject(const FDevNoteTag& Tag)
{
	TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	JsonObject->SetStringField(TEXT("id"), Tag.Id.ToString(EGuidFormats::DigitsWithHyphens));
	JsonObject->SetStringField(TEXT("name"), Tag.Name);
	JsonObject->SetNumberField(TEXT("colour"), Tag.Colour);
	return JsonObject;

}

bool UDevNoteSubsystem::ParseTagFromJsonObject(const TSharedPtr<FJsonObject>& JsonObj,
	FDevNoteTag& OutTag)
{
	if (!JsonObj.IsValid())
	{
		return false;
	}

	FString idString;
	FString nameString;
	int colour = 0;

	JsonObj->TryGetStringField(TEXT("id"), idString);
	JsonObj->TryGetStringField(TEXT("name"), nameString);
	JsonObj->TryGetNumberField(TEXT("colour"), colour);

	OutTag.Id = FGuid(idString);
	OutTag.Name = nameString;
	OutTag.Colour = colour;
	return true;
}

FString UDevNoteSubsystem::SerializeNoteToJsonString(const FDevNote& Note)
{
	TSharedPtr<FJsonObject> JsonObject = ConvertNoteToJsonObject(Note);
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	return OutputString;
}

void UDevNoteSubsystem::CreateNewNoteAtEditorLocation()
{
	TSharedPtr<FDevNote> newNote = MakeShared<FDevNote>();
	newNote->Title = "New Note";
	newNote->CreatedAt = FDateTime::UtcNow();
	newNote->Id = FGuid::NewGuid();
	newNote->LevelPath = GetCurrentLevelPath();
	newNote->WorldPosition = GetEditorViewportCameraLocation();

	CachedNotes.Add(newNote);
	PostNote(*newNote);
}

void UDevNoteSubsystem::SetEditorEditingState(bool bEditing)
{
	bIsEditorEditing = bEditing;
	if (!bIsEditorEditing && bRefreshPendingWhileEditing)
	{
		bRefreshPendingWhileEditing = false;
		RequestNotesFromServer();
	}
}

void UDevNoteSubsystem::HandleTagsResponse(TSharedPtr<IHttpRequest> HttpRequest, TSharedPtr<IHttpResponse> HttpResponse,
	bool bWasSuccessful)
{
	CachedTags.Empty();
	HandleTokenInvalidation(HttpResponse);

	// Parse tags
	if (bWasSuccessful && HttpResponse->GetResponseCode() == EHttpResponseCodes::Ok)
	{
		FString ResponseString = HttpResponse->GetContentAsString();
		TArray<TSharedPtr<FJsonValue>> JsonArray;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);
		if (FJsonSerializer::Deserialize(Reader, JsonArray))
		{
			for (const auto& Item : JsonArray)
			{
				FDevNoteTag Tag;
				if (ParseTagFromJsonObject(Item->AsObject(), Tag))
				{
					CachedTags.Add(Tag);
				}
			}
		}
	}
	
	OnTagsUpdated.Broadcast();
}

void UDevNoteSubsystem::RequestTagsFromServer()
{
	FHttpModule* Http = &FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();

	Request->OnProcessRequestComplete().BindUObject(this, &UDevNoteSubsystem::HandleTagsResponse);
	Request->SetURL(GetServerAddress() + TEXT("/tags"));
	Request->SetVerb("GET");
	Request->SetHeader("Content-Type", "application/json");
	Request->SetHeader(TEXT("X-Session-Token"), *SessionToken);
	Request->ProcessRequest();
}

UDevNoteSubsystem* UDevNoteSubsystem::Get()
{
	if (GEditor)
	{
		return GEditor->GetEditorSubsystem<UDevNoteSubsystem>();
	}
	return nullptr;
}

void UDevNoteSubsystem::ParseAndCacheNotesFromJson(const FString& JsonString)
{
	TArray<TSharedPtr<FJsonValue>> NotesArray;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

	if (FJsonSerializer::Deserialize(Reader, NotesArray))
	{
		CachedNotes.Empty();

		for (const TSharedPtr<FJsonValue>& Value : NotesArray)
		{
			TSharedPtr<FJsonObject> JsonObj = Value->AsObject();
			TSharedPtr<FDevNote> Note = MakeShared<FDevNote>();
			
			if (ParseNoteFromJsonObject(JsonObj, *Note))
			{
				CachedNotes.Add(Note);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("Failed to parse DevNote from JSON."));
			}
		}
	}
}

void UDevNoteSubsystem::HandleNotesResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	HandleTokenInvalidation(Response);

	if (!bWasSuccessful || !Response.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to get notes"));
		return;
	}

	TArray<TSharedPtr<FJsonValue>> JsonArray;
	const FString ResponseString = Response->GetContentAsString();
	
	ParseAndCacheNotesFromJson(ResponseString);
	OnNotesUpdated.Broadcast();

	// Delete all waypoints and recreate them
	RefreshWaypointActors();
}

FString UDevNoteSubsystem::GetServerAddress() const
{
	const UDevNotesDeveloperSettings* Settings = GetDefault<UDevNotesDeveloperSettings>();
	return Settings && !Settings->ServerAddress.IsEmpty() ? Settings->ServerAddress : TEXT("http://localhost:5281");
}


FString UDevNoteSubsystem::GetSessionTokenFilePath() const
{
	// Use the /Saved directory
	return FPaths::ProjectSavedDir() / SessionTokenFileName;
}

FString UDevNoteSubsystem::LoadSessionTokenFromFile() const
{
	FString Token;
	FFileHelper::LoadFileToString(Token, *GetSessionTokenFilePath());
	Token.TrimStartAndEndInline();
	return Token;
}

void UDevNoteSubsystem::SaveSessionTokenToFile(const FString& Token) const
{
	FFileHelper::SaveStringToFile(Token, *GetSessionTokenFilePath());
}

void UDevNoteSubsystem::DeleteSessionTokenFile() const
{
	IFileManager::Get().Delete(*GetSessionTokenFilePath());
}

void UDevNoteSubsystem::SetSessionToken(const FString& Token)
{
	SessionToken = Token;
	if (!Token.IsEmpty())
	{
		SaveSessionTokenToFile(Token);
	}
	else
	{
		DeleteSessionTokenFile();
	}
}

void UDevNoteSubsystem::ClearSessionToken()
{
	SessionToken.Empty();
	DeleteSessionTokenFile();
}

bool UDevNoteSubsystem::IsLoggedIn() const
{
	return !SessionToken.IsEmpty();
}

void UDevNoteSubsystem::HandleTokenInvalidation(TSharedPtr<IHttpResponse> HttpResponse)
{
	if (HttpResponse.IsValid())
	{
		const int32 ResponseCode = HttpResponse->GetResponseCode();
		
		// Only treat specific auth-related codes as token invalidation
		// Don't invalidate token for other error codes
		if (ResponseCode == EHttpResponseCodes::Denied || ResponseCode == EHttpResponseCodes::Forbidden)
		{
			UE_LOG(LogDevNotes, Warning, TEXT("Session token invalidated by server (code: %d). Signing out."), ResponseCode);
			SignOutInternal();
		}
	}
}

void UDevNoteSubsystem::SignOutInternal()
{
	const bool bWasLoggedIn = IsLoggedIn();
	
	// Clear token and cached data
	ClearSessionToken();
	CachedNotes.Empty();
	CachedTags.Empty();
	CurrentUserId.Invalidate();
	
	// Stop polling timer
	if (RefreshNotesTimerHandle.IsValid())
	{
		GEditor->GetTimerManager()->ClearTimer(RefreshNotesTimerHandle);
	}
	
	// Refresh UI
	RefreshWaypointActors();
	OnNotesUpdated.Broadcast();
	
	// Broadcast sign out event only if we were actually logged in
	if (bWasLoggedIn)
	{
		OnSignedOut.Broadcast();
	}
}

void UDevNoteSubsystem::SignIn(const FString& UserName, const FString& Password,
                               TFunction<void(bool bSuccess, const FString& Error)> Completion)
{
	TSharedRef<FJsonObject> RequestObj = MakeShared<FJsonObject>();
	RequestObj->SetStringField(TEXT("UserName"), UserName);
	RequestObj->SetStringField(TEXT("Password"), Password);
	
	FString Body;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(RequestObj, Writer);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(GetServerAddress() + TEXT("/signin"));
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(Body);

	HttpRequest->OnProcessRequestComplete().BindLambda(
		[this, Completion](FHttpRequestPtr Req, FHttpResponsePtr Response, bool bWasSuccessful)
		{
			if (bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200)
			{
				TSharedPtr<FJsonObject> JsonObj;
				const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
				if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid() && JsonObj->HasField(TEXT("token")))
				{
					const FString Token = JsonObj->GetStringField(TEXT("token"));
					SetSessionToken(Token);
					
					// Extract current user ID from response if available
					FString UserIdString;
					if (JsonObj->TryGetStringField(TEXT("Id"), UserIdString))
					{
						CurrentUserId = FGuid(UserIdString);
						UE_LOG(LogDevNotes, Log, TEXT("Current user ID set to: %s"), *CurrentUserId.ToString());
					}
					else
					{
						UE_LOG(LogDevNotes, Warning, TEXT("User ID not found in sign-in response"));
					}
					
					OnSignedIn.Broadcast(Token);
					
					if (Completion) 
					{
						Completion(true, FString());
					}
					return;
				}
			}
			
			// Handle failure
			FString ErrorMessage;
			if (Response.IsValid())
			{
				ErrorMessage = FString::Printf(TEXT("Sign in failed: %s"), *Response->GetContentAsString());
			}
			else
			{
				ErrorMessage = TEXT("No server response");
			}
			
			if (Completion)
			{
				Completion(false, ErrorMessage);
			}
		});
		
	HttpRequest->ProcessRequest();

}

void UDevNoteSubsystem::SignOut(TFunction<void(bool)> Completion)
{
	if (!IsLoggedIn())
	{
		// Already signed out
		if (Completion)
		{
			Completion(true);
		}
		return;
	}

	// Create HTTP request to sign out on server
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(GetServerAddress() + TEXT("/signout"));
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("X-Session-Token"), *SessionToken);

	TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	JsonObject->SetStringField(TEXT("sessionToken"), SessionToken);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	HttpRequest->SetContentAsString(SessionToken);
	
	HttpRequest->OnProcessRequestComplete().BindLambda(
		[this, Completion](FHttpRequestPtr Req, FHttpResponsePtr Response, bool bWasSuccessful)
		{
			// Always clear local session regardless of server response
			SignOutInternal();
			
			const bool bSuccess = bWasSuccessful && Response.IsValid() && Response->GetResponseCode() == 200;
			if (!bSuccess)
			{
				UE_LOG(LogDevNotes, Warning, TEXT("Server sign-out request failed, but local session cleared"));
			}
			
			if (Completion)
			{
				Completion(bSuccess);
			}
		});
		
	HttpRequest->ProcessRequest();
}

void UDevNoteSubsystem::RetryTokenValidation()
{
	if (!SessionToken.IsEmpty())
	{
		UE_LOG(LogDevNotes, Log, TEXT("Retrying token validation..."));
		
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
		Request->SetURL(GetServerAddress() + TEXT("/validatetoken"));
		Request->SetVerb(TEXT("POST"));
		Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
		Request->SetContentAsString(SessionToken);
		
		Request->OnProcessRequestComplete().BindLambda(
			[this](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bWasSuccessful)
			{
				if (bWasSuccessful && HttpResponse.IsValid())
				{
					const int32 ResponseCode = HttpResponse->GetResponseCode();
					UE_LOG(LogDevNotes, Log, TEXT("Token validation response: %d"), ResponseCode);
				
					if (ResponseCode == EHttpResponseCodes::Ok)
					{
						// Token is valid, extract user id from response
						TSharedPtr<FJsonObject> JsonObj;
						const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(HttpResponse->GetContentAsString());

						if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
						{
							FString UserIdString;
							if (JsonObj->TryGetStringField(TEXT("Id"), UserIdString))
							{
								CurrentUserId = FGuid(UserIdString);
								UE_LOG(LogDevNotes, Log, TEXT("Current user ID restored: %s"), *CurrentUserId.ToString());
							}
						}
					
						UE_LOG(LogDevNotes, Log, TEXT("Session token validated successfully"));
						OnSignedIn.Broadcast(SessionToken);
					}
					else if (ResponseCode == EHttpResponseCodes::Denied || ResponseCode == EHttpResponseCodes::Forbidden)
					{
						UE_LOG(LogDevNotes, Warning, TEXT("Session token rejected by server (code: %d) - clearing"), ResponseCode);
						ClearSessionToken();
						OnSignedOut.Broadcast();
					}
					else
					{
						UE_LOG(LogDevNotes, Warning, TEXT("Server error during token validation (code: %d) - keeping token for retry"), ResponseCode);
					}
				}
				else
				{
					UE_LOG(LogDevNotes, Warning, TEXT("Failed to validate token due to network error - keeping token for retry"));
				}

			});

		Request->ProcessRequest();
	}
}


ADevNoteActor* UDevNoteSubsystem::SpawnWaypointForNote(TSharedPtr<FDevNote> Note)
{
	if (!GEditor) return nullptr;

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return nullptr;

	auto settings = GetDefault<UDevNotesDeveloperSettings>();
	auto spawnClass = settings->DevNoteActorRepresentation.LoadSynchronous();
	
	FActorSpawnParameters SpawnParams;
	// Name waypoint after the title of the note
	SpawnParams.Name = MakeUniqueObjectName(World, spawnClass, FName(*FString::Printf(TEXT("DevNote_%s"), *Note->Id.ToString())));
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	
	// Don't save or dirty the world state
	SpawnParams.ObjectFlags |= RF_Transient;

	ADevNoteActor* Waypoint = Cast<ADevNoteActor>(World->SpawnActor(spawnClass, &Note->WorldPosition, &FRotator::ZeroRotator, SpawnParams));
	if (Waypoint)
	{
		Waypoint->Note = Note;
		Waypoint->SetActorLabel(TEXT("DevNote ") + Note->Title, false);
		Waypoint->SetIsTemporarilyHiddenInEditor(false);

		// Prevents above modifications from triggering edit events on the actor, prompting note updates
		Waypoint->bReadyForSync = true;
	}

	return Waypoint;
}

void UDevNoteSubsystem::ClearAllNoteWaypoints()
{
	if (!GEditor) return;
	
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return;

	// Could cache the actors instead
	for (TActorIterator<ADevNoteActor> It(World); It; ++It)
	{
		GEditor->SelectActor(*It, false, true);
		It->Destroy();
	}
}

void UDevNoteSubsystem::RefreshWaypointActors()
{
	// Cache ID's
	StoreSelectedNoteIDs();
	ClearAllNoteWaypoints();

	// Cache viewport selection
	TArray<AActor*> selection;
	
	auto CurrentLevels = GetLoadedLevelPaths();
	for (auto Note : CachedNotes)
	{
		auto levelString = Note->LevelPath.GetLongPackageName();
		if (!CurrentLevels.Contains(levelString)) continue;
		auto spawned = SpawnWaypointForNote(Note);
		if (!spawned) continue;

		if (SelectedNoteIDsBeforeRefresh.Contains(Note->Id))
		{
			selection.Add(spawned);
		}
	}

	// Restore selection
	for (auto actor : selection)
	{
		GEditor->SelectActor(actor, true, true);
	}
}

TArray<TSharedPtr<FDevNote>> UDevNoteSubsystem::GetSelectedNoteWaypoints()
{
	TArray<TSharedPtr<FDevNote>> SelectedNotes;
	USelection* selection = GEditor->GetSelectedActors();
	TArray<AActor*> selectionArray;
	selection->GetSelectedObjects(selectionArray);
	for (AActor* Actor : selectionArray)
	{
		ADevNoteActor* Waypoint = Cast<ADevNoteActor>(Actor);
		if (Waypoint)
		{
			SelectedNotes.Add(Waypoint->Note);
		}
	}
	return SelectedNotes;
}

void UDevNoteSubsystem::StoreSelectedNoteIDs()
{
	SelectedNoteIDsBeforeRefresh.Empty();
	TArray<TSharedPtr<FDevNote>> SelectedNotes = GetSelectedNoteWaypoints();
	for (TSharedPtr<FDevNote> Note : SelectedNotes)
	{
		if (Note)
		{
			SelectedNoteIDsBeforeRefresh.Add(Note->Id);
		}
	}
}

void UDevNoteSubsystem::PostTag(FDevNoteTag NoteTag)
{
	FHttpModule* Http = &FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
	Request->SetURL(GetServerAddress() + "/tags");
	Request->SetVerb("POST");
	Request->SetHeader("Content-Type", "application/json");
	Request->SetHeader(TEXT("X-Session-Token"), *SessionToken);
	
	TSharedPtr<FJsonObject> JsonObject = ConvertTagToJsonObject(NoteTag);
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	
	Request->SetContentAsString(OutputString);
	Request->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr Req, FHttpResponsePtr Response, bool bSuccess)
	{
		HandleTokenInvalidation(Response);

		if (bSuccess && Response->GetResponseCode() == EHttpResponseCodes::Created)
		{
			UE_LOG(LogDevNotes, Log, TEXT("Tag created successfully."));
		}
	});

	Request->ProcessRequest();
}

void UDevNoteSubsystem::DeleteTag(const FGuid& TagId)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(GetServerAddress() + "/tags/" + TagId.ToString(EGuidFormats::DigitsWithHyphens));
	Request->SetVerb("DELETE");
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("X-Session-Token"), *SessionToken);

	Request->OnProcessRequestComplete().BindLambda([this](FHttpRequestPtr Req, FHttpResponsePtr Response, bool bSuccess)
	{
		HandleTokenInvalidation(Response);

		if (bSuccess && (Response->GetResponseCode() == EHttpResponseCodes::Ok || Response->GetResponseCode() == EHttpResponseCodes::NoContent))
		{
			UE_LOG(LogDevNotes, Log, TEXT("Tag deleted successfully."));
			// Refresh tags from server after successful deletion
			RequestTagsFromServer();
		}
		else
		{
			if (Response)
			{
				UE_LOG(LogDevNotes, Error, TEXT("Failed to delete tag: %s. Error: %d"), *Response->GetContentAsString(), Response->GetResponseCode());
			}
			else
			{
				UE_LOG(LogDevNotes, Error, TEXT("Could not get a response from server: %s"), *Req->GetURL());
			}
		}
	});

	Request->ProcessRequest();

}

bool UDevNoteSubsystem::ParseUserFromJsonObject(const TSharedPtr<FJsonObject>& JsonObj, FDevNoteUser& OutUser)
{
	if (!JsonObj.IsValid())
	{
		return false;
	}

	FString idString;
	FString nameString;

	JsonObj->TryGetStringField(TEXT("id"), idString);
	JsonObj->TryGetStringField(TEXT("name"), nameString);

	OutUser.Id = FGuid(idString);
	OutUser.Name = nameString;
	return true;
}

const FDevNoteUser& UDevNoteSubsystem::GetUserById(const FGuid& UserId)
{
	auto foundUser =  CachedUsers.FindByPredicate([UserId](const FDevNoteUser& user)
	{
		return user.Id == UserId;
	});

	if (!foundUser)
	{
		static FDevNoteUser EmptyUser;
		return EmptyUser;
	}

	return *foundUser;
}


void UDevNoteSubsystem::HandleUsersResponse(TSharedPtr<IHttpRequest> HttpRequest,
                                            TSharedPtr<IHttpResponse> HttpResponse, bool bWasSuccessful)
{
	CachedUsers.Empty();
	HandleTokenInvalidation(HttpResponse);

	if (bWasSuccessful && HttpResponse->GetResponseCode() == EHttpResponseCodes::Ok)
	{
		FString ResponseString = HttpResponse->GetContentAsString();
		TArray<TSharedPtr<FJsonValue>> JsonArray;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseString);
		if (FJsonSerializer::Deserialize(Reader, JsonArray))
		{
			for (const auto& Item : JsonArray)
			{
				FDevNoteUser User;
				if (ParseUserFromJsonObject(Item->AsObject(), User))
				{
					CachedUsers.Add(User);
				}
			}
		}
	}
}

void UDevNoteSubsystem::RequestUsersFromServer()
{
	FHttpModule* Http = &FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();
	
	Request->OnProcessRequestComplete().BindUObject(this, &UDevNoteSubsystem::HandleUsersResponse);
	Request->SetURL(GetServerAddress() + TEXT("/users"));
	Request->SetVerb("GET");
	Request->SetHeader("Content-Type", "application/json");
	Request->SetHeader(TEXT("X-Session-Token"), *SessionToken);
	Request->ProcessRequest();
}
