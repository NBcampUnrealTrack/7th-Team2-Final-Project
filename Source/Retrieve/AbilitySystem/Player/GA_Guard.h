#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Player/GA_ParryBase.h"
#include "GA_Guard.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitInputRelease;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class UGameplayEffect;
/** 방패(SwordShield) 전용 — 막기(홀드 블록) + 패리. */
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

	void StopRuntimeTasks();

private:
	UPROPERTY(EditDefaultsOnly, Category = "Guard")
	TSoftObjectPtr<UAnimMontage> GuardMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Guard")
	TSubclassOf<UGameplayEffect> GuardBreakStaggerEffect;

	UPROPERTY(Transient) TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;
	UPROPERTY(Transient) TObjectPtr<UAbilityTask_WaitInputRelease> InputReleaseTask;
	UPROPERTY(Transient) TObjectPtr<UAbilityTask_WaitGameplayEvent> GuardBrokenTask;
};
