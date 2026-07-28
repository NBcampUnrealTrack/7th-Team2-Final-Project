// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "GA_LockOn.generated.h"

/**
 * 락온 토글 어빌리티
 * Tab 입력으로 1회 활성화되어 UCombatReactionComponent::TryToggleLockOn() 호출 후 종료
 * 락온 상태는 LockOnComponent가 영속 보유하고 GA는 행위만 책임
 */
UCLASS()
class RETRIEVE_API UGA_LockOn : public URetrieveGameplayAbility
{
	GENERATED_BODY()
	
public:
	UGA_LockOn();
	
protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, 
		const FGameplayAbilityActorInfo* ActorInfo, 
		const FGameplayAbilityActivationInfo ActivationInfo, 
		const FGameplayEventData* TriggerEventData) override;
	
};
