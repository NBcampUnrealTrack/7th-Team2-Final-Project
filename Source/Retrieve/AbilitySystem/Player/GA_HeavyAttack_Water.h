#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Player/GA_HeavyAttack_Base.h"
#include "GA_HeavyAttack_Water.generated.h"

class UGameplayEffect;

/**
 * Water 강공격 — 주변 적에게 AoE 피해 + Cold(빙결) 상태 부여
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
	void ApplyAoEColdDamage();

private:
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|HeavyAttack|Water")
	TSubclassOf<UGameplayEffect> DamageEffectClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|HeavyAttack|Water")
	TSubclassOf<UGameplayEffect> ColdEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|HeavyAttack|Water", meta = (ClampMin = "0.0"))
	float SweepRadius = 350.f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|HeavyAttack|Water", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 1.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|HeavyAttack|Water")
	bool bDebugDrawSweep = false;
};
