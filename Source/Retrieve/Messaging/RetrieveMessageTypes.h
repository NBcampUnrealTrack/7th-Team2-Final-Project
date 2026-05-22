#pragma once

#include "CoreMinimal.h"
#include "Core/RetrieveSessionState.h"
#include "RetrieveMessageTypes.generated.h"

USTRUCT(BlueprintType)
struct FRetrieveSessionStatePayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Session")
	ERetrieveSessionState PreviousState = ERetrieveSessionState::Loading;

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Session")
	ERetrieveSessionState NewState = ERetrieveSessionState::Loading;
};
