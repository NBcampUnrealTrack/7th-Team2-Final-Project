// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTagContainer.h"
#include "RetrieveArenaBlockActor.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UMaterialInstanceDynamic;
class UPrimitiveComponent;
struct FEnemyPlayerSpottedPayload;
struct FMonsterDiedPayload;

/**
 * 보스 아레나 결계 액터.
 *
 * 보스가 플레이어를 처음 인지해도 바로 결계를 올리지 않고, 플레이어가 EntryTrigger
 * (아레나 안쪽 입구를 지난 지점에 배치하는 박스 볼륨) 안으로 실제로 들어왔을 때만
 * 결계를 올려 구역 이탈을 막습니다. 보스가 처치되면 결계를 해제합니다.
 *
 * 거리/반경 기반 판정(플레이어-보스 거리, 아레나 중심 반경 등)은 방 형태와 크기에 따라
 * 값이 계속 달라져 안정적이지 않았기 때문에, 디자이너가 직접 배치하는 Overlap 트리거로
 * 대체했습니다 - "플레이어가 이 지점을 지났다"를 코드가 추측하지 않고 레벨에서 직접 정의합니다.
 *
 * 보스/AI 코드와 직접 결합하지 않고 기존 Gameplay Message만 구독합니다.
 *  - 감지: Channel.Enemy.PlayerSpotted (FRetrieveEnemyTargetEvaluator가 첫 인지 시 발행)
 *  - 해제: Channel.Monster.Died        (HandleDeathStarted에서 발행)
 *
 * 보스는 EnemySpawner로 런타임에 생성되므로 인스턴스 참조 대신
 * 클래스(BossClass)로 판별합니다. 같은 클래스 보스가 여러 구역에 있으면
 * ArenaRadius 거리 필터로 구분합니다.
 *
 * 에디터에서 반드시 설정할 항목:
 *  - BossClass   : 이 아레나가 감시할 보스 BP 클래스
 *  - ArenaRadius : 같은 클래스 보스가 여러 구역에 있을 때 구분용 반경 (0이면 거리 체크 안 함)
 *  - EntryTrigger: 아레나 입구를 지나 안쪽에 배치할 진입 판정 박스 (결계 메시 전체보다 작게,
 *                  입구를 지나 아레나 내부로 살짝 들어온 위치에 둘 것)
 *  - Barrier     : 결계 메시/콜리전 (Pawn Block 권장)
 */
UCLASS()
class RETRIEVE_API ARetrieveArenaBlockActor : public AActor
{
	GENERATED_BODY()

public:
	ARetrieveArenaBlockActor();

	/** 피격 링 연출 트리거. 결계 히트를 감지한 쪽(무기 트레이스 등)에서 히트 월드 좌표로 호출. */
	UFUNCTION(BlueprintCallable, Category="Boss Arena")
	void TriggerHitRipple(const FVector& HitWorldLocation);

	/** 결계 활성화(잠금) 순간. BP에서 활성화 사운드 + 유지 루프 시작에 사용. */
	UFUNCTION(BlueprintImplementableEvent, Category="Boss Arena|FX")
	void OnArenaActivated();

	/** 결계에 무언가 닿은 순간(쿨다운 통과 시). BP에서 히트 사운드 재생에 사용. */
	UFUNCTION(BlueprintImplementableEvent, Category="Boss Arena|FX")
	void OnArenaHit(const FVector& HitLocation);

	/** 결계 해제(보스 처치) 순간. BP에서 유지 루프 정지 + 해제 사운드에 사용. */
	UFUNCTION(BlueprintImplementableEvent, Category="Boss Arena|FX")
	void OnArenaDeactivated();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void LockArena();
	void UnlockArena();

	/** 디졸브 종료 후 실제로 메시를 숨김 (UnlockArena 타이머 콜백) */
	void HideBarrier();

private:
	/** 보스가 플레이어를 처음 인지한 순간 → 결계를 바로 걸지 않고 EntryTrigger 진입을 기다린다 */
	void OnPlayerSpotted(FGameplayTag Channel, const FEnemyPlayerSpottedPayload& Payload);

	/** 보스 사망 → 결계 해제 */
	void OnMonsterDied(FGameplayTag Channel, const FMonsterDiedPayload& Payload);

	/** 메시지에 실려 온 액터/위치가 이 아레나의 보스인지 판별 */
	bool IsArenaBoss(const AActor* Actor, const FVector& Location) const;

	/** 캐시된 보스의 BossHPBarComponent를 표시/숨김 */
	void SetBossHPBarVisible(bool bVisible);

	/** 플레이어가 EntryTrigger 안으로 들어오면 결계를 잠근다 */
	UFUNCTION()
	void OnEntryTriggerBeginOverlap(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	/** 결계에 무언가 부딪히면(플레이어/적/무기 충돌) 그 지점에 피격 링 */
	UFUNCTION()
	void OnBarrierHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

protected:
	/** 이 아레나가 감시할 보스 클래스 (스포너가 런타임에 스폰하므로 클래스로 판별) */
	UPROPERTY(EditAnywhere, Category="Boss Arena")
	TSubclassOf<AActor> BossClass;

	/** 같은 클래스 보스가 여러 구역에 있을 때 구분용. 아레나 중심 기준 반경(cm), 0이면 거리 체크 안 함 */
	UPROPERTY(EditAnywhere, Category="Boss Arena", meta=(ClampMin="0.0"))
	float ArenaRadius = 0.f;

	/** 피격 링 최소 간격(초). 결계에 밀착했을 때 링이 매 프레임 재시작되는 것 방지 */
	UPROPERTY(EditAnywhere, Category="Boss Arena", meta=(ClampMin="0.0"))
	float RippleCooldown = 0.3f;

	/** 결계 소멸(디졸브) 지속시간(초). 보스 처치 시 이 시간에 걸쳐 사라진 뒤 숨김. */
	UPROPERTY(EditAnywhere, Category="Boss Arena", meta=(ClampMin="0.0"))
	float UnlockDuration = 0.6f;

	UPROPERTY(VisibleAnywhere, Category="Boss Arena")
	TObjectPtr<USceneComponent> Root;

	/** 결계 메시 + 콜리전 (잠금 시 Pawn Block) */
	UPROPERTY(VisibleAnywhere, Category="Boss Arena")
	TObjectPtr<UStaticMeshComponent> Barrier;

	/**
	 * 플레이어가 이 트리거 안으로 들어오면 결계를 잠근다.
	 * 에디터에서 아레나 입구를 지나 안쪽으로 살짝 들어온 위치/크기로 배치할 것
	 * (결계 메시 전체보다 작게 잡아야 "들어온 뒤"에만 반응한다).
	 */
	UPROPERTY(VisibleAnywhere, Category="Boss Arena")
	TObjectPtr<UBoxComponent> EntryTrigger;

private:
	FGameplayMessageListenerHandle SpottedHandle;
	FGameplayMessageListenerHandle DiedHandle;

	/** 디졸브 종료 후 숨김 타이머 */
	FTimerHandle HideTimerHandle;

	/** 보스가 처음 인지한 플레이어 - EntryTrigger 오버랩 시 이 액터인지 확인용 */
	TWeakObjectPtr<AActor> PendingSpottedPlayer;

	/** 보스가 플레이어를 인지해서 EntryTrigger 진입을 기다리는 중인지 */
	bool bWaitingForPlayerEntry = false;

	/** 결계FX 런타임 파라미터(LockTime/HitLocation/HitTime) 제어용 MID */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BarrierMID;

	/** 마지막 피격 링 시각 (RippleCooldown 스로틀용) */
	float LastRippleTime = -1000.f;

	/** 현재 활성화(잠금) 상태 — 중복 활성화/해제 이벤트 방지 */
	bool bIsLocked = false;

	/** 보스 처치 후 재진입 방지 */
	bool bCleared = false;
	/** 이 아레나의 보스 인스턴스 (PlayerSpotted 시 캐시 → HP바 Show/Hide 대상) */
	TWeakObjectPtr<AActor> CachedBoss;
};
