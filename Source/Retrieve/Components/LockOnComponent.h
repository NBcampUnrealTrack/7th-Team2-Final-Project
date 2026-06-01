
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "GameplayTagContainer.h"
#include "LockOnComponent.generated.h"

class APlayerController;
class APlayerCameraManager;
class APawn;
class ULockOnConfig;
class UAbilitySystemComponent;   

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLockOnTargetChanged, AActor*, NewTarget);

/**
 * 카메라 기준 SphereTrace로 후보를 모으고 화면 중앙 가중치 + 거리 가중치로 베스트 타겟을 고르는 컴포넌트
 * 락온 활성화 시 ASC에 LockOn_Active 태그 부여, 거리/시야/사망을 감시해 자동 해제
 */

UCLASS(ClassGroup = "Retrieve", meta=(BlueprintSpawnableComponent))
class RETRIEVE_API ULockOnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULockOnComponent();
	
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	// 튜닝 파라미터 주입
	void SetConfig(ULockOnConfig* InConfig);
	// 베스트 후보가 있다면 락온 시작
	UFUNCTION(BlueprintCallable, Category = "Retrieve|LockOn")
	bool StartLockOn();
	// 현재 락온 해제 (태그 제거 + 델리게이트 nullptr 브로드캐스트)
	UFUNCTION(BlueprintCallable, Category = "Retrieve|LockOn")
	void StopLockOn();
	// 좌우/상하 입력 방향으로 인접 타겟으로 스위치 Tab(LockOn버튼)과 별도의 바인드 필요 - 검토 필요
	UFUNCTION(BlueprintCallable, Category = "Retrieve|LockOn")
	bool SwitchTarget(FVector2D InputDir);
	// LockOn 토글
	UFUNCTION(BlueprintCallable, Category = "Retrieve|LockOn")
	bool Toggle();
	UFUNCTION(BlueprintPure, Category = "Retrieve|LockOn")
	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }
	
	UFUNCTION(BlueprintPure, Category = "Retrieve|LockOn")
	bool IsLockedOn() const { return bLockOnActive && CurrentTarget.IsValid(); }
	// 타겟이 바뀔 때 새 타겟 을 Broadcast
	UPROPERTY(BlueprintAssignable, Category = "Retrieve|LockOn")
	FOnLockOnTargetChanged OnTargetChanged;
	
protected:
	// 튜닝 파라미터 DataAsset(ReactionComponent에서 주입)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|LockOn")
	TObjectPtr<ULockOnConfig> Config;
	
private:
	// MonitorTimer 콜백 락온 유지 가능 여부 체크
	void MonitorTick();
	// 디버그 드로우 — 현재 타겟 시각화
	void DrawDebugLockOn() const;
	// Helpers
	APawn* GetOwningPawn() const;
	bool CachePlayerRefs();
	void FindCandidates(TArray<AActor*>& OutCandidates) const;
	bool HasLineOfSight(const AActor* Target) const;
	bool ProjectToScreen(const AActor* Actor, FVector2D& OutScreen, bool& bOutInFront) const;
	AActor* PickBestTarget(const TArray<AActor*>& Candidates) const;
	AActor* PickSwitchTarget(const TArray<AActor*>& Candidates, FVector2D InputDir) const;
	void SetCurrentTarget(AActor* NewTarget);
	bool ShouldBreakLockOn() const;
	bool IsTargetDead(const AActor* Target) const;
	void ApplyLockOnTag(bool bAdd) const;
	// 타겟 사망 태그 이벤트 구독/해제 (폴링 대체)
	void SubscribeTargetDeath(AActor* Target);
	void UnsubscribeTargetDeath();
	void OnTargetDeadTagChanged(const FGameplayTag Tag, int32 Count);
	
	// State
	TWeakObjectPtr<AActor> CurrentTarget;
	bool bLockOnActive = false;
	TWeakObjectPtr<APlayerController> CachedPC;
	TWeakObjectPtr<APlayerCameraManager> CachedCameraManager;
	
	FTimerHandle MonitorTimerHandle; 
	// 사망 이벤트 구독 상태
	TWeakObjectPtr<UAbilitySystemComponent> SubscribedASC;
	FDelegateHandle EnemyDeadHandle;
	FDelegateHandle BossDeadHandle;
};
