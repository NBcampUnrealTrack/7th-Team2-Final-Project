#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Player/GA_HeavyAttack_Base.h"
#include "GA_HeavyAttack_Water.generated.h"

class UGameplayEffect;

/**
 * Water 강공격 — 방어막 생성
 */
UCLASS()
class RETRIEVE_API UGA_HeavyAttack_Water : public UGA_HeavyAttack_Base
{
	GENERATED_BODY()

public:
	UGA_HeavyAttack_Water();

protected:
	virtual void ExecuteHeavyEffect(const FGameplayTag& ConsumedElement) override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|HeavyAttack|Water")
	TSubclassOf<UGameplayEffect> ShieldEffectClass;
};
