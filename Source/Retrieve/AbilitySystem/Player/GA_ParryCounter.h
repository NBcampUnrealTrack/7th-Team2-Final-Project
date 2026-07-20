#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GA_ParryCounter.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UGameplayEffect;
class UWeaponComponent;

/**
 * 패리 성공 후 발동하는 카운터 어빌리티
 */
UCLASS()
class RETRIEVE_API UGA_ParryCounter : public URetrieveGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_ParryCounter();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;

private:
	void StopRuntimeTasks();

	AActor* ResolveCounterTarget() const;
	void RegisterCounterWarpTarget();
	void ClearCounterWarpTarget();

	// DamageScale=창별 데미지 배수(ANS_AttackImpact). 그로기는 첫 히트(찌르기)에만, 넉백은 이후(마무리) 히트에만.
	void ApplyCounterToTarget(AActor* TargetActor, float DamageScale, bool bFirstHit);

	bool TryApplyMonsterGroggy(AActor* TargetActor, float Duration) const;

	UFUNCTION() void HandleImpactEvent(FGameplayEventData Payload);
	UFUNCTION() void HandleMontageCompleted();
	UFUNCTION() void HandleMontageInterrupted();
	UFUNCTION() void HandleMontageCancelled();

private:
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|ParryCounter")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|ParryCounter|MotionWarping", meta = (ClampMin = "0.0"))
	float CounterWarpStandoffOffset = 50.f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|ParryCounter|MotionWarping", meta = (ClampMin = "0.0"))
	float CounterMaxWarpDistance = 500.f;

	UPROPERTY(Transient)
	FRetrieveWeaponDataRow CachedWeaponData;

	// 발동 시점에 확정한 무기별 카운터 데이터(배율/그로기/넉백/리액트/피드백).
	UPROPERTY(Transient)
	FParryCounterData CachedParryCounterData;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CachedCounterTarget;

	// 카운터 콤보 내 임팩트 히트 수(0=첫 히트).
	UPROPERTY(Transient)
	int32 CounterHitIndex = 0;

	// 타겟 뒤 구도 블렌드 속도(클수록 빠름).
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|ParryCounter|Camera", meta = (ClampMin = "0.1"))
	float CounterCameraBlendSpeed = 8.f;

	UPROPERTY(Transient)
	TObjectPtr<UWeaponComponent> CachedWeaponComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ImpactEventTask;
};
