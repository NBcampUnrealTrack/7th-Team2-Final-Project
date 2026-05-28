#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "GA_Die.generated.h"

class UAbilityTask_PlayMontageAndWait;
/**
 * 
 */
UCLASS()
class RETRIEVE_API UGA_Die : public URetrieveGameplayAbility
{
	GENERATED_BODY()
	
public:
	UGA_Die();

protected:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

private:
	UFUNCTION() void HandleMontageCompleted();
	UFUNCTION() void HandleMontageCancelled();
	UFUNCTION() void HandleMontageInterrupted();
	UFUNCTION() void HandleMontageBlendOut();

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Death",
		meta = (DisplayName = "사망 몽타주 (FullBody 슬롯 배정)"))
	TObjectPtr<UAnimMontage> DeathMontage;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;
};
