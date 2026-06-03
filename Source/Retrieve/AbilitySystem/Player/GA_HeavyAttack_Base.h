#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "GA_HeavyAttack_Base.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;

/**
 * 우클릭 강공격(원소 게이지 1칸 소모)의 추상 베이스
 *
 * 모드 분기는 자식의 ActivationRequiredTags(Element.Fire/Water/Wind)로 라우팅
 *  - 3개 자식 모두 동일 우클릭 InputTag로 부여 → ASC가 셋 다 활성 시도
 *  - 현재 원소 모드와 일치하는 하나만 ActivationRequiredTags를 통과 → 자동 분기
 */
UCLASS(Abstract)
class RETRIEVE_API UGA_HeavyAttack_Base : public URetrieveGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_HeavyAttack_Base();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	
	virtual void ExecuteHeavyEffect(const FGameplayTag& ConsumedElement);
	
	void PlayHeavyMontageThenEnd();
	
	void ExecuteOwnerCue(const FGameplayTag& CueTag) const;

protected:
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
