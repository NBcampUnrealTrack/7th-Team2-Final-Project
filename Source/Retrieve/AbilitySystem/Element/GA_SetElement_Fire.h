#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "GA_SetElement_Fire.generated.h"

/**
 * 
 */
UCLASS()
class RETRIEVE_API UGA_SetElement_Fire : public URetrieveGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_SetElement_Fire();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;
};
