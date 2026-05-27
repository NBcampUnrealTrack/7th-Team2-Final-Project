#pragma once

#include "CoreMinimal.h"
#include "Core/RetrieveSessionState.h"
#include "GameplayTagContainer.h"
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

USTRUCT(BlueprintType)
struct FRetrieveElementGaugeFullPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|ElementGauge")
	TObjectPtr<AActor> Instigator = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|ElementGauge")
	TArray<FGameplayTag> FilledElements;
};
