#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Player/GA_ParryBase.h"
#include "GA_Guard.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitInputRelease;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class UGameplayEffect;

/**
 * 방패(SwordShield) 전용 홀드 가드.
 *
 * 이 Ability는 누르고 있는 동안 Guard 상태와 stamina drain만 담당한다.
 * 검방패 패리 시도는 GA_GuardAttack + AnimNotifyState_ParryWindow 쪽으로 분리했다.
 */
UCLASS()
class RETRIEVE_API UGA_Guard : public UGA_ParryBase
{
	GENERATED_BODY()

public:
	UGA_Guard();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	UFUNCTION() void HandleInputReleased(float TimeHeld);
	UFUNCTION() void HandleGuardBroken(FGameplayEventData Payload);
	UFUNCTION() void HandleGuardStaminaTick();

	void StopRuntimeTasks();

private:
	UPROPERTY(EditDefaultsOnly, Category = "Guard")
	TSoftObjectPtr<UAnimMontage> GuardMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Guard")
	TSubclassOf<UGameplayEffect> GuardBreakStaggerEffect;

	// 가드 지속 소모 점검 주기(초). 실제 소모량(초당)은 DT_StaminaCost 행의 DrainPerSecond.
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Cost", meta = (ClampMin = "0.01"))
	float StaminaCostTickInterval = 0.1f;

	UPROPERTY(Transient) TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;
	UPROPERTY(Transient) TObjectPtr<UAbilityTask_WaitInputRelease> InputReleaseTask;
	UPROPERTY(Transient) TObjectPtr<UAbilityTask_WaitGameplayEvent> GuardBrokenTask;

	FTimerHandle GuardStaminaTimerHandle;
};
