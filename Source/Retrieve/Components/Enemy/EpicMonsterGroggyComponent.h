#pragma once

#include "CoreMinimal.h"
#include "Components/PawnComponent.h"
#include "GameplayEffectTypes.h"
#include "EpicMonsterGroggyComponent.generated.h"

class URetrieveAbilitySystemComponent;

/** 강공격 누적 비율 또는 그로기 잔여 비율(1→0)과 상태를 UI에 전달 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnGroggyGaugeUpdated, float, Ratio, bool, bIsGroggyActive);

/**
 * 에픽 몬스터 전용 그로기 게이지 컴포넌트.
 * 강공격(Attack_Type_Heavy 태그를 가진 GE)을 HitsRequired 회 받으면 그로기를 트리거한다.
 * UI 바인딩은 OnGroggyGaugeUpdated 델리게이트로 처리한다.
 */
UCLASS(ClassGroup = "Retrieve", meta = (BlueprintSpawnableComponent))
class RETRIEVE_API UEpicMonsterGroggyComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	UEpicMonsterGroggyComponent(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** UI가 구독하는 게이지 업데이트 델리게이트 */
	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Groggy")
	FOnGroggyGaugeUpdated OnGroggyGaugeUpdated;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Groggy")
	bool IsGroggyActive() const { return bGroggyActive; }

	/** 현재 강공격 누적 비율 (0.0 ~ 1.0) */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Groggy")
	float GetChargeRatio() const;

	void ApplyGroggyState(float Duration);

	/** 재조우(리스폰/재활성) 시 그로기 누적·타이머·쿨다운을 초기화한다. */
	void ResetRespawnState();

private:
	void OnAbilitySystemInitialized();

	/** ASC에 GE가 적용될 때 호출 — Attack_Type_Heavy 태그 여부 확인 */
	void HandleGameplayEffectApplied(UAbilitySystemComponent* Source,
		const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle);

	/** 강공격 카운트 증가 및 그로기 트리거 판정 */
	void OnHeavyAttackReceived();

	/** 그로기 GameplayEvent 발생 */
	void TriggerGroggy();
	void ClearGroggyState();

	/** State_Enemy_Groggy 태그 변화 콜백 — 그로기 종료 감지용 */
	void OnGroggyTagChanged(const FGameplayTag Tag, int32 NewCount);

	URetrieveAbilitySystemComponent* GetASC() const;

	/** 그로기 지속 시간 (초) */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Groggy", meta = (ClampMin = "1.0"))
	float GroggyDuration = 8.f;

	/** 그로기 트리거에 필요한 강공격 적중 횟수 */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Groggy", meta = (ClampMin = "1"))
	int32 HitsRequired = 3;

	/** 그로기 종료 후 재진입 대기 시간 (초) */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Groggy", meta = (ClampMin = "0.0"))
	float GroggyCooldown = 10.f;

	int32 HeavyAttackCount = 0;
	bool bGroggyActive = false;
	float ActiveGroggyDuration = 0.f;
	float GroggyEndTime = 0.f;
	float GroggyCooldownExpiry = 0.f;

	FDelegateHandle GEAppliedHandle;
	FDelegateHandle GroggyTagHandle;
	FTimerHandle GroggyEndTimerHandle;
};
