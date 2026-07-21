#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RetrieveGameplayMessageTypes.generated.h"

class UTexture2D;

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

/**
 * Channel.UI.PickupToast 페이로드.
 * 인벤토리에 들어가지 않는 획득(퀘스트 물건 회수 등)을 아이템 획득 토스트로
 * 시각 피드백하기 위해 발행. ToastManager(WBP_ToastManager)가 구독해 커스텀 토스트를 띄운다.
 */
USTRUCT(BlueprintType)
struct RETRIEVE_API FRetrievePickupToastPayload
{
	GENERATED_BODY()

	/** 토스트에 표시할 이름/문구. */
	UPROPERTY(BlueprintReadOnly)
	FText Title;

	/** 토스트에 표시할 아이콘 텍스처(맵 아이콘 재활용 등). 없으면 아이콘 미표시. */
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UTexture2D> Icon = nullptr;

	/** 수량 표시 문구(예: "+100", "×3"). 비어 있으면 수량 숨김. */
	UPROPERTY(BlueprintReadOnly)
	FText QuantityText;
};
