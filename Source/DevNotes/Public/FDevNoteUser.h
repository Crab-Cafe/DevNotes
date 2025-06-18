#pragma once
#include "FDevNoteUser.generated.h"

USTRUCT()
struct DEVNOTES_API FDevNoteUser : public FTableRowBase 
{
    GENERATED_BODY()

    UPROPERTY()
    FGuid Id;
    
    UPROPERTY()
    FString Name;
};