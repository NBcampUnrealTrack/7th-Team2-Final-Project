
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/TimerHandle.h"
#include "LockOnComponent.generated.h"

class APlayerController;
class APlayerCameraManager;
class APawn;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLockOnTargetChanged, AActor*, NewTarget);

/**
 * 카메라 기준 SphereTrace로 후보를 모으고 화면 중앙 가중치 + 거리 가중치로 베스트 타겟을 고르는 컴포넌트
 * 락온 활성화 시 ASC에 LockOn_Active 태그 부여, 거리/시야/사망을 감시해 자동 해제
 */

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RETRIEVE_API ULockOnComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULockOnComponent();
	
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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
	// Search params (1주차 검증 후 ULockOnConfig DataAsset으로 분리 예정)
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|LockOn|Search")
	float MaxDistance = 2500.f;
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|LockOn|Search")
	float SphereRadius = 600.f;
	// SphereTrace 대상 기본은 Pawn
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|LockOn|Search")
	TArray<TEnumAsByte<EObjectTypeQuery>> SearchObjectTypes;
	// 시야 검증에 쓸 채널 기본은 Visibility
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|LockOn|Search")
	TEnumAsByte<ECollisionChannel> LineOfSightChannel = ECC_Visibility;
	// Scoring weights
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|LockOn|Scoring", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ScreenWeight = 0.7f;
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|LockOn|Scoring", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DistanceWeight = 0.3f;
	// 화면 중앙으로부터 정규화 거리 이 이상이면 후보 제외
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|LockOn|Scoring", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxScreenDistance = 0.8f;
	// Switch / break
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|LockOn|Switch")
	float SwitchInputThreshold = 0.5f;
	// 현재 타겟과의 거리가 이 값을 넘으면 자동 해제
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|LockOn|Break")
	float BreakDistance = 3000.f;
	// 자동 해제 체크 주기(초). 0.1 = 10Hz.
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|LockOn|Break", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float MonitorInterval = 0.1f;
	// Debug
	UPROPERTY(EditAnywhere, Category = "Retrieve|LockOn|Debug")
	bool bDebugDraw = true;

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
	
	// State
	TWeakObjectPtr<AActor> CurrentTarget;
	bool bLockOnActive = false;
	TWeakObjectPtr<APlayerController> CachedPC;
	TWeakObjectPtr<APlayerCameraManager> CachedCameraManager;
	
	FTimerHandle MonitorTimerHandle; 
};
