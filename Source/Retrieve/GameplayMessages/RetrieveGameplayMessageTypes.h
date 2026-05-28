#pragma once

#include "CoreMinimal.h"
#include "RetrieveGameplayMessageTypes.generated.h"

/**
 * Channel.Monster.Died 페이로드.
 * 적 사망 시 AEnemyCharacter::HandleDeathStarted 가 발행.
 * 퀘스트·킬 카운터·E 시스템 등 외부 시스템이 구독.
 */
USTRUCT(BlueprintType)
struct RETRIEVE_API FMonsterDiedPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<AActor> DeadActor;

	UPROPERTY(BlueprintReadOnly)
	FVector DeathLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<AActor> Killer;
	
	UPROPERTY(BlueprintReadOnly) 
	TWeakObjectPtr<AActor> DamageCauser;

	/** DT_MonsterData Row 이름. 분류·집계용. 미연동 시 NAME_None */
	UPROPERTY(BlueprintReadOnly)
	FName MonsterDataRow;
};

USTRUCT(BlueprintType)
struct RETRIEVE_API FPlayerDiedPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) 
	TWeakObjectPtr<AActor> DeadActor;
	
	UPROPERTY(BlueprintReadOnly) 
	FVector DeathLocation = FVector::ZeroVector;
	
	UPROPERTY(BlueprintReadOnly) 
	TWeakObjectPtr<AActor> Killer;
	
	UPROPERTY(BlueprintReadOnly) 
	TWeakObjectPtr<AActor> DamageCauser;
};

/**
 * Channel.Enemy.PlayerSpotted 페이로드.
 * 적이 플레이어를 발견한 시점에 FRetrieveEnemyTargetEvaluator 가 발행.
 * 인접 적이 InstigatorLocation 기준 AlertRadius 거리 필터로 구독해 군집 알림 처리.
 */
USTRUCT(BlueprintType)
struct RETRIEVE_API FEnemyPlayerSpottedPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<AActor> SpottedActor;

	UPROPERTY(BlueprintReadOnly)
	FVector SpottedLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	FVector InstigatorLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<AActor> InstigatorEnemy;
};
