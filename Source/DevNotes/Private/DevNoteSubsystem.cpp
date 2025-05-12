// Fill out your copyright notice in the Description page of Project Settings.


#include "DevNoteSubsystem.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Runtime/JsonUtilities/Public/JsonObjectConverter.h"

#define LIVE_URL_BASE "http://localhost:5281";

void UDevNoteSubsystem::FetchNotes()
{
	FString uriBase = LIVE_URL_BASE;
	FString uriLogin = uriBase + TEXT("/notes");

	FHttpModule& httpModule = FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> pRequest = httpModule.CreateRequest();
	pRequest->SetVerb(TEXT("GET"));
	pRequest->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));
	//FString RequestContent = TEXT("identity=") + NewUser + TEXT("&password=") + NewPassword + TEXT("&query=") + uriQuery;
	//pRequest->SetContentAsString(RequestContent);
	pRequest->SetURL(uriLogin);

	pRequest->OnProcessRequestComplete().BindLambda([&](
			FHttpRequestPtr pRequest,
			FHttpResponsePtr pResponse,
			bool connectedSuccessfully) mutable {

		if (connectedSuccessfully) {


			
			// We should have a JSON response - attempt to process it.
			//ProcessSpaceTrackResponse(pResponse->GetContentAsString());
			UE_LOG(LogTemp, Error, TEXT("This is the data: %s"), *pResponse->GetContentAsString());
			
			FString JSONPayload = pResponse->GetContentAsString();  // The actual payload
			FDevNote note;
			FJsonObjectConverter::JsonObjectStringToUStruct(JSONPayload, &note, 0, 0);
			
		}
		else {
			switch (pRequest->GetStatus()) {
			case EHttpRequestStatus::Failed_ConnectionError:
				UE_LOG(LogTemp, Error, TEXT("Connection failed."));
			default:
				UE_LOG(LogTemp, Error, TEXT("Request failed."));
			}
		}
	});

	// Finally, submit the request for processing
	pRequest->ProcessRequest();
}

void UDevNoteSubsystem::PushNote(const FDevNote& note)
{
	FString JSONPayload;
	FJsonObjectConverter::UStructToJsonObjectString(note, JSONPayload, 0, 0);

	FHttpModule& httpModule = FHttpModule::Get();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> pRequest = httpModule.CreateRequest();
	pRequest->SetVerb(TEXT("POST"));
	pRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	pRequest->SetContentAsString(JSONPayload);
	
	FString uriBase = LIVE_URL_BASE;
	FString uriLogin = uriBase + TEXT("/pushnote");
	pRequest->SetURL(uriLogin);

	// Finally, submit the request for processing
	pRequest->ProcessRequest();


}
