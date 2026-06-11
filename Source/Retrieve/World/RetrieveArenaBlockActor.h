// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTagContainer.h"
#include "RetrieveArenaBlockActor.generated.h"

class UStaticMeshComponent;
struct FEnemyPlayerSpottedPayload;
struct FMonsterDiedPayload;

/**
 * 보스 아레나 결계 액터.
 *
 * 보스가 플레이어를 처음 인지하면 결계를 올려 구역 이탈을 막고,
 * 보스가 처치되면 결계를 해제합니다.
 *
 * 보스/AI 코드와 직접 결합하지 않고 기존 Gameplay Message만 구독합니다.
 *  - 잠금: Channel.Enemy.PlayerSpotted (FRetrieveEnemyTargetEvaluator가 첫 인지 시 발행)
 *  - 해제: Channel.Monster.Died        (HandleDeathStarted에서 발행)
 *
 * 보스는 EnemySpawner로 런타임에 생성되므로 인스턴스 참조 대신
 * 클래스(BossClass)로 판별합니다. 같은 클래스 보스가 여러 구역에 있으면
 * ArenaRadius 거리 필터로 구분합니다.
 *
 * 에디터에서 반드시 설정할 항목:
 *  - BossClass   : 이 아레나가 감시할 보스 BP 클래스
 *  - ArenaRadius : 아레나 중심 기준 판별 반경 (0이면 거리 체크 안 함)
 *  - Barrier     : 결계 메시/콜리전 (Pawn Block 권장)
 */
UCLASS()
class RETRIEVE_API ARetrieveArenaBlockActor : public AActor
{
	GENERATED_BODY()

public:
	ARetrieveArenaBlockActor();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void LockArena();
	void UnlockArena();

private:
	/** 보스가 플레이어를 처음 인지한 순간 → 결계 잠금 */
	void OnPlayerSpotted(FGameplayTag Channel, const FEnemyPlayerSpottedPayload& Payload);

	/** 보스 사망 → 결계 해제 */
	void OnMonsterDied(FGameplayTag Channel, const FMonsterDiedPayload& Payload);

	/** 메시지에 실려 온 액터/위치가 이 아레나의 보스인지 판별 */
	bool IsArenaBoss(const AActor* Actor, const FVector& Location) const;

protected:
	/** 이 아레나가 감시할 보스 클래스 (스포너가 런타임에 스폰하므로 클래스로 판별) */
	UPROPERTY(EditAnywhere, Category="Boss Arena")
	TSubclassOf<AActor> BossClass;

	/** 같은 클래스 보스가 여러 구역에 있을 때 구분용. 아레나 중심 기준 반경(cm), 0이면 거리 체크 안 함 */
	UPROPERTY(EditAnywhere, Category="Boss Arena", meta=(ClampMin="0.0"))
	float ArenaRadius = 0.f;

	UPROPERTY(VisibleAnywhere, Category="Boss Arena")
	TObjectPtr<USceneComponent> Root;

	/** 결계 메시 + 콜리전 (잠금 시 Pawn Block) */
	UPROPERTY(VisibleAnywhere, Category="Boss Arena")
	TObjectPtr<UStaticMeshComponent> Barrier;

private:
	FGameplayMessageListenerHandle SpottedHandle;
	FGameplayMessageListenerHandle DiedHandle;

	/** 보스 처치 후 재진입 방지 */
	bool bCleared = false;
};
