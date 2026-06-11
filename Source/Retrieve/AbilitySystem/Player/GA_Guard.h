#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "GA_Guard.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitInputRelease;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class UGameplayEffect;
/**
 * 
 */
UCLASS()
class RETRIEVE_API UGA_Guard : public URetrieveGameplayAbility
{
	GENERATED_BODY()
	
public:
	UGA_Guard();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

public:
	AActor* GetLastParriedAttacker() const { return LastParriedAttacker.Get(); }

private:
	UFUNCTION() void HandleInputReleased(float TimeHeld);
	UFUNCTION() void HandleGuardBroken(FGameplayEventData Payload);
	UFUNCTION() void HandleParrySuccess(FGameplayEventData Payload);

	void StopRuntimeTasks();

private:
	UPROPERTY(EditDefaultsOnly, Category = "Guard")
	TSoftObjectPtr<UAnimMontage> GuardMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Guard")
	TSubclassOf<UGameplayEffect> GuardBreakStaggerEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Guard|Parry")
	TSubclassOf<UGameplayEffect> ParryWindowEffect;
	
	UPROPERTY(EditDefaultsOnly, Category = "Guard|Parry")
	TSubclassOf<UGameplayEffect> ParryCooldownEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Guard|Parry")
	TSubclassOf<UGameplayEffect> CounterWindowEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Guard|Parry")
	TSubclassOf<UGameplayEffect> ParryStaggerEffect;

	UPROPERTY(Transient) TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;
	UPROPERTY(Transient) TObjectPtr<UAbilityTask_WaitInputRelease> InputReleaseTask;
	UPROPERTY(Transient) TObjectPtr<UAbilityTask_WaitGameplayEvent> GuardBrokenTask;
	UPROPERTY(Transient) TObjectPtr<UAbilityTask_WaitGameplayEvent> ParrySuccessTask;
	
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> LastParriedAttacker;
};
