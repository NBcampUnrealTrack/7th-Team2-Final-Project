#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Enemy/GAS/GA_EnemyPatternAbilityBase.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GA_Enemy_DashAttack.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;
class UAnimSequenceBase;
class UEnemyCombatComponent;

UCLASS(Blueprintable, BlueprintType)
class RETRIEVE_API UGA_Enemy_DashAttack : public UGA_EnemyPatternAbilityBase
{
	GENERATED_BODY()

public:
	UGA_Enemy_DashAttack(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

protected:
	virtual void OnMontageCompleted() override;
	virtual void OnMontageInterrupted() override;
	
private:
	UAnimMontage* ResolveAttackMontage(const FGameplayEventData* TriggerEventData,
		const FMonsterPatternRow* PatternRow,
		float DynamicMontagePlayRate) const;

	void FinishAbility(bool bWasCancelled);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dash Attack", meta = (ClampMin = "0.01"))
	float MontagePlayRate = 1.f;
	
private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;
};
