#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
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

/**
 * Channel.Enemy.SpawnGroupCleared 페이로드.
 * 스포너(ASpawnerBase)의 살아있는 스폰 집합이 모두 사망한 경우 발행.
 * 퀘스트, 구역 게이트 등 외부 시스템이 SpawnGroupId로 필터링해 해당 구역 클리어를 처리한다.
 * 스포너는 퀘스트에 의존하지 않으므로 Quest.Step을 알지 못한다. 매핑은 구독자 쪽에서 한다.
 */
USTRUCT(BlueprintType)
struct RETRIEVE_API FSpawnGroupClearedPayload
{
	GENERATED_BODY()

	/** 스포너 그룹 식별자. 구독자가 매칭에 사용. */
	UPROPERTY(BlueprintReadOnly)
	FGameplayTag SpawnGroupId;

	/** 신호를 발행한 스포너. */
	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<AActor> Spawner;
};
