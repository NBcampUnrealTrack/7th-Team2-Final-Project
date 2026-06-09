#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "GameplayTagContainer.h"
#include "GA_Absorb.generated.h"

class UGameplayEffect;

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
	/** 원소 태그 → 적용할 GE */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Absorb")
	TMap<FGameplayTag, TSubclassOf<UGameplayEffect>> ElementToAbsorbEffect;

	/** 원소 태그 → 버프 바에 표시할 UI.Buff.Absorb.* 태그
	 *  비어 있는 원소는 버프 UI를 표시하지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Absorb")
	TMap<FGameplayTag, FGameplayTag> ElementToAbsorbBuffUITag;
};
