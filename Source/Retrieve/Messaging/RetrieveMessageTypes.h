#pragma once

#include "CoreMinimal.h"
#include "Core/RetrieveSessionState.h"
#include "GameplayTagContainer.h"
#include "RetrieveMessageTypes.generated.h"

// ---- 세션 ---------------------------------------------------------

USTRUCT(BlueprintType)
struct FRetrieveSessionStatePayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Session")
	ERetrieveSessionState PreviousState = ERetrieveSessionState::Loading;

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Session")
	ERetrieveSessionState NewState = ERetrieveSessionState::Loading;
};

// ---- 전투 ---------------------------------------------------------

USTRUCT(BlueprintType)
struct FRetrieveElementGaugeFullPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|ElementGauge")
	TObjectPtr<AActor> Instigator = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|ElementGauge")
	TArray<FGameplayTag> FilledElements;
};

// ---- 루멘 ---------------------------------------------------------

/** 루멘 Follow/Positioning 모드. 호스트만 쓸 수 있습니다. ULumenFollowComponent에서 Replicate 됩니다. */
UENUM(BlueprintType)
enum class EFollowMode : uint8
{
	Follow,
	Wait,
	RetreatCombat
};

/** OnRep_Mode마다 Channel.Lumen.Mode.Changed로 브로드캐스트됩니다. */
USTRUCT(BlueprintType)
struct FRetrieveLumenModePayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Lumen")
	EFollowMode Mode = EFollowMode::Follow;
};

/** Channel.Lumen.Command.ToggleWait와 Channel.Lumen.Command.Recall의 공유 페이로드. */
USTRUCT(BlueprintType)
struct FRetrieveLumenCommandPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Lumen")
	FGameplayTag CommandTag;

	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Lumen")
	TObjectPtr<AActor> Instigator = nullptr;
};

// ---- 퀘스트 -------------------------------------------------------------------

/** Channel.Quest.StepChanged / Channel.Quest.SealUnlocked 페이로드. */
USTRUCT(BlueprintType)
struct FRetrieveQuestStepPayload
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Quest")
	FGameplayTag StepTag;
};

/** Channel.Quest.GuardianDefeated 페이로드. */
USTRUCT(BlueprintType)
struct FRetrieveGuardianDefeatedPayload
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintReadWrite, Category = "Retrieve|Quest")
	FGameplayTag GuardianElement;
};