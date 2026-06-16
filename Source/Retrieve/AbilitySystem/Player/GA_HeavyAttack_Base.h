#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "GA_HeavyAttack_Base.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;

/**
 * 강공격(원소 게이지 1칸 소모)의 추상 베이스
 */
UCLASS(Abstract)
class RETRIEVE_API UGA_HeavyAttack_Base : public URetrieveGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_HeavyAttack_Base();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	
	virtual void ExecuteHeavyEffect(const FGameplayTag& ConsumedElement);
	
	void PlayHeavyMontageThenEnd();
	
	void ExecuteOwnerCue(const FGameplayTag& CueTag) const;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|HeavyAttack")
	bool bActivateForStaff = false;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|HeavyAttack")
	TSoftObjectPtr<UAnimMontage> HeavyMontage;
	
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|HeavyAttack", meta = (ClampMin = "0.1"))
	float MontagePlayRate = 1.0f;

private:
	UFUNCTION() 
	void HandleMontageFinished();

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;
};
