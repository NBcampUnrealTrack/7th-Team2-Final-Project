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
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	UEnemyCombatComponent* GetEnemyCombatComponent() const;
	const FMonsterPatternRow* GetActivePatternRow() const;

	UFUNCTION(BlueprintPure)
	float GetAttackSpeedMultiplier() const;
	
	UFUNCTION(BlueprintPure)
	float GetAttackMontagePlayRate(float BasePlayRate = 1.f) const;

	UFUNCTION()
	virtual void OnMontageCompleted();

	UFUNCTION()
	virtual void OnMontageInterrupted();

private:
	void CleanupAttachedPatternVFX(const FGameplayAbilityActorInfo* ActorInfo) const;
};
