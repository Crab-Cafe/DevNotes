// Fill out your copyright notice in the Description page of Project Settings.


#include "DevNoteSubsystem.h"

#include "DevNotesLog.h"
#include "EngineUtils.h"
#include "HttpModule.h"
#include "JsonObjectConverter.h"
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

void UDevNoteSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UDevNoteSubsystem::RequestNotesFromServer()
{
	FHttpModule* Http = &FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = Http->CreateRequest();

	Request->OnProcessRequestComplete().BindUObject(this, &UDevNoteSubsystem::HandleNotesResponse);
	Request->SetURL(GetServerAddress() + TEXT("/notes"));
	Request->SetVerb("GET");
	Request->SetHeader("Content-Type", "application/json");
	Request->ProcessRequest();
	
}

void UDevNoteSubsystem::PostNote(const FDevNote& Note)
{
	FString JsonString = SerializeNoteToJsonString(Note);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(GetServerAddress() + "/notes");
	Request->SetVerb("POST");
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(JsonString);

	Request->OnProcessRequestComplete().BindLambda([](FHttpRequestPtr Req, FHttpResponsePtr Response, bool bSuccess)
	{
		if (bSuccess && Response->GetResponseCode() == 201)
		{
			UE_LOG(LogDevNotes, Log, TEXT("Note posted successfully."));
		}
		else
		{
			if (Response)
			{
				UE_LOG(LogDevNotes, Error, TEXT("Failed to post note: %s"), *Response->GetContentAsString());
			}
			else
			{
				UE_LOG(LogDevNotes, Error, TEXT("Could not get a response from server: %s"), *Req->GetURL());
			}
		}
	});

	Request->ProcessRequest();
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

	JsonObj->TryGetStringField(TEXT("levelPath"), OutNote.LevelPath);

	FString dateTimeString;
	JsonObj->TryGetStringField(TEXT("createdAt"), dateTimeString);
	FDateTime::ParseIso8601(*dateTimeString, OutNote.CreatedAt);

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

	JsonObject->SetStringField(TEXT("levelPath"), Note.LevelPath);
	JsonObject->SetStringField(TEXT("createdAt"), Note.CreatedAt.ToIso8601());

	return JsonObject;
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
	FDevNote newNote;
	newNote.Title = "New Note";
	newNote.CreatedAt = FDateTime::UtcNow();
	newNote.Id = FGuid::NewGuid();
	newNote.LevelPath = GetCurrentLevelPath();
	newNote.WorldPosition = GetEditorViewportCameraLocation();

	CachedNotes.Add(newNote);
	PostNote(newNote);
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

			FDevNote Note;
			
			if (ParseNoteFromJsonObject(JsonObj, Note))
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


inline void UDevNoteSubsystem::SpawnWaypointForNote(const FDevNote& Note)
{
	if (!GEditor) return;

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return;

	auto settings = GetDefault<UDevNotesDeveloperSettings>();
	auto spawnClass = settings->DevNoteActorRepresentation.LoadSynchronous();
	
	FActorSpawnParameters SpawnParams;
	SpawnParams.Name = MakeUniqueObjectName(World, spawnClass, FName(*FString::Printf(TEXT("DevNote_%s"), *Note.Id.ToString())));
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// Don't save or dirty the world state
	SpawnParams.ObjectFlags |= RF_Transient;
	

	AActor* Waypoint = World->SpawnActor(spawnClass, &Note.WorldPosition, &FRotator::ZeroRotator, SpawnParams);
	if (Waypoint)
	{
		//Waypoint->NoteId = FGuid(Note.Id);
		Waypoint->SetActorLabel(TEXT("DevNote Waypoint"), false);
		Waypoint->SetIsTemporarilyHiddenInEditor(false);
	}
}

void UDevNoteSubsystem::ClearAllNoteWaypoints()
{
	if (!GEditor) return;

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World) return;

	for (TActorIterator<ADevNoteActor> It(World); It; ++It)
	{
		It->Destroy();
	}
}

void UDevNoteSubsystem::RefreshWaypointActors()
{
	ClearAllNoteWaypoints();
	
	auto CurrentLevels = GetLoadedLevelPaths();
	for (const FDevNote& Note : CachedNotes)
	{
		if (!CurrentLevels.Contains(Note.LevelPath)) continue;
		SpawnWaypointForNote(Note);
	}
}