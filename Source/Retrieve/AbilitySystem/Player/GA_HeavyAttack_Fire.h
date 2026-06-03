#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Player/GA_HeavyAttack_Base.h"
#include "GA_HeavyAttack_Fire.generated.h"

class UGameplayEffect;

/**
 * Fire 강공격 — 폭발 타격(광역)
 */
UCLASS()
class RETRIEVE_API UGA_HeavyAttack_Fire : public UGA_HeavyAttack_Base
{
	GENERATED_BODY()

public:
	UGA_HeavyAttack_Fire();

protected:
	virtual void ExecuteHeavyEffect(const FGameplayTag& ConsumedElement) override;

private:
	void ApplyRadialDamage();

private:
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|HeavyAttack|Fire")
	TSubclassOf<UGameplayEffect> DamageEffectClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|HeavyAttack|Fire", meta = (ClampMin = "0.0"))
	float SweepRadius = 300.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|HeavyAttack|Fire", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|HeavyAttack|Fire")
	bool bDebugDrawSweep = false;
};
