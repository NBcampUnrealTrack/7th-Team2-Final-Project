#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Enemy/GAS/GA_EnemyPatternAbilityBase.h"
#include "GA_Enemy_Acceleration.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class UGameplayEffect;

UCLASS(Blueprintable, BlueprintType)
class RETRIEVE_API UGA_Enemy_Acceleration : public UGA_EnemyPatternAbilityBase
{
	GENERATED_BODY()

public:
	UGA_Enemy_Acceleration(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

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
	UAnimMontage* ResolveMontage(const FGameplayEventData* TriggerEventData) const;
	void StartListeningForCountered();
	void ApplyAccelerationEffect();
	void FinishAbility(bool bWasCancelled);

	UFUNCTION()
	void OnCountered(FGameplayEventData Payload);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Acceleration")
	TSubclassOf<UGameplayEffect> AccelerationEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Acceleration", meta = (ClampMin = "0.01"))
	float MontagePlayRate = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Acceleration|Counter")
	FGameplayTag CounteredEventTag;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> CounterTask;

	bool bCountered = false;
};
