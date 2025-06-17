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
#include "DevNotes/DevNotesDeveloperSettings.h"
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
		// Get the current persistent level package name (full path)
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
		// Filter for perspective viewport (not orthographic) and Level Editor type
		if (ViewportClient && ViewportClient->IsPerspective() && ViewportClient->IsVisible())
		{
			return ViewportClient->GetViewLocation();
		}
	}
#endif
	return FVector::ZeroVector;
}


void UDevNoteSubsystem::Tick(float DeltaTime)
{
}

void UDevNoteSubsystem::OnPollNotesTimer()
{
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

	// Set the token optimistically - we'll clear it only if server explicitly rejects it
	SessionToken = SavedToken;
	UE_LOG(LogDevNotes, Log, TEXT("Loaded session token from file, validating with server..."));

	// Create HTTP request to validate token
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(GetServerAddress() + TEXT("/validatetoken"));
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(SavedToken);

	// Response handler
	Request->OnProcessRequestComplete().BindLambda(
		[this, SavedToken](FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bWasSuccessful)
		{
			if (bWasSuccessful && HttpResponse.IsValid())
			{
				const int32 ResponseCode = HttpResponse->GetResponseCode();
				UE_LOG(LogDevNotes, Log, TEXT("Token validation response: %d"), ResponseCode);
				
				if (ResponseCode == 200)
				{
					// Token is valid - we're already set up, just broadcast the event
					UE_LOG(LogDevNotes, Log, TEXT("Session token validated successfully"));
					OnSignedIn.Broadcast(SavedToken);
				}
				else if (ResponseCode == 401 || ResponseCode == 403)
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
				// This prevents losing tokens when the server is temporarily unavailable
				UE_LOG(LogDevNotes, Warning, TEXT("Failed to validate token due to network error - keeping token for retry"));
			}
		});

	Request->ProcessRequest();
	return true;
}

void UDevNoteSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	// Try to restore session from saved token
	TryAutoSignIn();
	
	// Set up signed-in event handler
	OnSignedIn.AddWeakLambda(this, [this](FString Token)
	{
		UE_LOG(LogDevNotes, Log, TEXT("Signed in successfully, starting data sync..."));
		RequestTagsFromServer();
		RequestNotesFromServer();

		// Start polling timer
		GEditor->GetTimerManager()->SetTimer(
			RefreshNotesTimerHandle,
			this,
			&UDevNoteSubsystem::OnPollNotesTimer,
			30.0f,
			true);
	});
}

void UDevNoteSubsystem::RequestNotesFromServer()
{
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

		if (bSuccess && Response->GetResponseCode() == 201)
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

		if (bSuccess && Response->GetResponseCode() == static_cast<int>(EHttpServerResponseCodes::Ok))
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

		if (bSuccess && Response->GetResponseCode() == 201)
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
		bool validLevelPath = !TargetLevelPath.IsEmpty() && !TargetLevelPath.StartsWith("/Temp/");
		if (!validLevelPath)
		{
				success = false;
		}
		else
		{
			
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

	// Extract required fields safely
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
	
	// Optional: Extract position
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
			FString TagIdStr = Val->AsObject()->GetStringField("id");
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
	JsonObject->SetNumberField(TEXT("color"), Tag.Colour);
	return JsonObject;

}

bool UDevNoteSubsystem::ParseTagFromJsonObject(const TSharedPtr<FJsonObject>& JsonObj,
	FDevNoteTag& OutTag)
{
	if (!JsonObj.IsValid())
	{
		return false;
	}

	// Extract required fields safely
	FString idString;
	FString nameString;
	int colour = 0;

	JsonObj->TryGetStringField(TEXT("id"), idString);
	JsonObj->TryGetStringField(TEXT("name"), nameString);
	JsonObj->TryGetNumberField(TEXT("color"), colour);

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

	if (bWasSuccessful && HttpResponse->GetResponseCode() == 200)
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

void UDevNoteSubsystem::ParseNotesFromJson(const FString& JsonString)
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
	
	ParseNotesFromJson(ResponseString);
	OnNotesUpdated.Broadcast();
	
	RefreshWaypointActors();
}

FString UDevNoteSubsystem::GetServerAddress() const
{
	const UDevNotesDeveloperSettings* Settings = GetDefault<UDevNotesDeveloperSettings>();
	return Settings && !Settings->ServerAddress.IsEmpty() ? Settings->ServerAddress : TEXT("http://localhost:5281");
}

// Authentication and Token Management Methods

FString UDevNoteSubsystem::GetSessionTokenFilePath() const
{
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

// Also update the token validation to be more robust
void UDevNoteSubsystem::HandleTokenInvalidation(TSharedPtr<IHttpResponse> HttpResponse)
{
	if (HttpResponse.IsValid())
	{
		const int32 ResponseCode = HttpResponse->GetResponseCode();
		
		// Only treat specific auth-related codes as token invalidation
		if (ResponseCode == 401 || ResponseCode == 403)
		{
			UE_LOG(LogDevNotes, Warning, TEXT("Session token invalidated by server (code: %d). Signing out."), ResponseCode);
			SignOutInternal();
		}
		// Don't invalidate token for other error codes (500, network errors, etc.)
	}
}

void UDevNoteSubsystem::SignOutInternal()
{
	const bool bWasLoggedIn = IsLoggedIn();
	
	// Clear token and cached data
	ClearSessionToken();
	CachedNotes.Empty();
	CachedTags.Empty();
	
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
	// Prepare JSON payload
	TSharedRef<FJsonObject> RequestObj = MakeShared<FJsonObject>();
	RequestObj->SetStringField(TEXT("UserName"), UserName);
	RequestObj->SetStringField(TEXT("Password"), Password);
	
	FString Body;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(RequestObj, Writer);

	// Create HTTP request
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(GetServerAddress() + TEXT("/signin"));
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(Body);

	// Response handler
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

// Add a method to manually retry validation if needed
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
				if (bWasSuccessful && HttpResponse.IsValid() && HttpResponse->GetResponseCode() == 200)
				{
					UE_LOG(LogDevNotes, Log, TEXT("Token validation retry successful"));
					OnSignedIn.Broadcast(SessionToken);
				}
				else if (bWasSuccessful && HttpResponse.IsValid() && (HttpResponse->GetResponseCode() == 401 || HttpResponse->GetResponseCode() == 403))
				{
					UE_LOG(LogDevNotes, Warning, TEXT("Token validation retry failed - token is invalid"));
					ClearSessionToken();
					OnSignedOut.Broadcast();
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


		Waypoint->bReadyForSync = true;
	}

	return Waypoint;
}

void UDevNoteSubsystem::ClearAllNoteWaypoints()
{
	if (!GEditor) return;
	
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return;

	for (TActorIterator<ADevNoteActor> It(World); It; ++It)
	{
		GEditor->SelectActor(*It, false, true);
		It->Destroy();
	}
}

void UDevNoteSubsystem::RefreshWaypointActors()
{
	StoreSelectedNoteIDs();
	ClearAllNoteWaypoints();

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

	for (auto actor : selection)
	{
		GEditor->SelectActor(actor, true, true);
	}
}

TArray<TSharedPtr<FDevNote>> UDevNoteSubsystem::GetSelectedNotes()
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
	TArray<TSharedPtr<FDevNote>> SelectedNotes = GetSelectedNotes();
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

		if (bSuccess && Response->GetResponseCode() == 201)
		{
			UE_LOG(LogDevNotes, Log, TEXT("Tag created successfully."));
		}
	}
	);

	Request->ProcessRequest();
}