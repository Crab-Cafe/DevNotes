// Fill out your copyright notice in the Description page of Project Settings.


#include "DevNotesRuntimeFunctionLibrary.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Engine/Level.h"
#include "UObject/Package.h"


bool UDevNotesRuntimeFunctionLibrary::UploadNote(FString Server, FString AuthKey, FString AsUser, FString Title,
                                                 FString Body, FVector Location, TSoftObjectPtr<UWorld> Level, const TArray<FString>& Tags)
{
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Server + "/notes/dto");
	Request->SetVerb("POST");
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("X-Session-Token"), *AuthKey);

	TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();

	JsonObject->SetStringField(TEXT("id"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens));
	JsonObject->SetStringField(TEXT("title"), Title);
	JsonObject->SetStringField(TEXT("body"), Body);

	JsonObject->SetNumberField(TEXT("worldX"), Location.X);
	JsonObject->SetNumberField(TEXT("worldY"), Location.Y);
	JsonObject->SetNumberField(TEXT("worldZ"), Location.Z);

	
	JsonObject->SetStringField(TEXT("levelPath"), !Level.IsNull() ? Level.GetLongPackageName() : FString());

	TArray<TSharedPtr<FJsonValue>> TagIdsArray;
	for (const FString& TagId : Tags)
	{
		TagIdsArray.Add(MakeShared<FJsonValueString>(TagId));
	}
	
	JsonObject->SetArrayField(TEXT("tagNames"), TagIdsArray);
	JsonObject->SetStringField(TEXT("createdByUserName"), AsUser);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);
	
	Request->SetContentAsString(OutputString);

	return Request->ProcessRequest();
}

TSoftObjectPtr<UWorld> UDevNotesRuntimeFunctionLibrary::GetActorLevelAsset(AActor* Actor)
{
	if (!Actor || !Actor->GetLevel())
	{
		return nullptr;
	}
    
	ULevel* Level = Actor->GetLevel();
	UWorld* World = Level->GetWorld();
    
	if (!World)
	{
		return nullptr;
	}
    
	// Get the package name of the world asset
	FString PackageName = World->GetPackage()->GetName();
	FString CleanPackageName = UWorld::RemovePIEPrefix(PackageName);

	return TSoftObjectPtr<UWorld>(FSoftObjectPath(CleanPackageName));
}

