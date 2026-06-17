#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "GA_EnemyPatternAbilityBase.generated.h"

class UEnemyCombatComponent;
struct FMonsterPatternRow;

UCLASS(Abstract, Blueprintable, BlueprintType)
class RETRIEVE_API UGA_EnemyPatternAbilityBase : public URetrieveGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_EnemyPatternAbilityBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	UEnemyCombatComponent* GetEnemyCombatComponent() const;
	const FMonsterPatternRow* GetActivePatternRow() const;

	float GetAttackSpeedMultiplier() const;
	float GetAttackMontagePlayRate(float BasePlayRate = 1.f) const;

	UFUNCTION()
	virtual void OnMontageCompleted();

	UFUNCTION()
	virtual void OnMontageInterrupted();
};
