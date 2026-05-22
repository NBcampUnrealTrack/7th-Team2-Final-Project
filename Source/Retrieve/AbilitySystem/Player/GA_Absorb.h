#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "GA_Absorb.generated.h"

class UGameplayEffect;

/**
 * 
 */
UCLASS()
class RETRIEVE_API UGA_Absorb : public URetrieveGameplayAbility
{
	GENERATED_BODY()
	
public:
	UGA_Absorb();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

public:
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Absorb")
	TMap<FGameplayTag, TSubclassOf<UGameplayEffect>> ElementToAbsorbEffect;
};
