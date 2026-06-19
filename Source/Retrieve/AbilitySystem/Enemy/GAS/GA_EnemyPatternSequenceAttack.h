#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GA_EnemyPatternSequenceAttack.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;
class UEnemyCombatComponent;

UCLASS(Blueprintable, BlueprintType)
class RETRIEVE_API UGA_EnemyPatternSequenceAttack : public URetrieveGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_EnemyPatternSequenceAttack(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

private:
	const FMonsterPatternRow* ResolveActivePatternRow() const;
	UAnimMontage* ResolveAttackMontage(const FGameplayEventData* TriggerEventData, const FMonsterPatternRow* PatternRow) const;
	void ScheduleHitboxWindow(const FMonsterPatternRow& PatternRow);
	void FinishAbility();

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageInterrupted();

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	FTimerHandle HitboxStartTimerHandle;
	FTimerHandle HitboxEndTimerHandle;
	FTimerHandle FinishTimerHandle;
};
