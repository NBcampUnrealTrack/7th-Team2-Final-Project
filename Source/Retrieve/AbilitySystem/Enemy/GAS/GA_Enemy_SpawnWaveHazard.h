#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Enemy/GAS/GA_EnemyPatternAbilityBase.h"
#include "GA_Enemy_SpawnWaveHazard.generated.h"

class AEnemyWaveHazard;
class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;

UCLASS(Blueprintable, BlueprintType)
class RETRIEVE_API UGA_Enemy_SpawnWaveHazard : public UGA_EnemyPatternAbilityBase
{
	GENERATED_BODY()

public:
	UGA_Enemy_SpawnWaveHazard(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

protected:
	virtual void OnMontageCompleted() override;
	virtual void OnMontageInterrupted() override;

private:
	UAnimMontage* ResolveAttackMontage(const FGameplayEventData* TriggerEventData) const;
	void ScheduleWaveSpawn();
	void SpawnWave();
	bool ResolveGroundLocation(const FVector& RequestedLocation, FVector& OutGroundLocation) const;
	void TryFinishAbility();
	void FinishAbility(bool bWasCancelled);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wave", meta=(ClampMin="0.01"))
	float MontagePlayRate = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wave|Spawn")
	TSubclassOf<AEnemyWaveHazard> WaveHazardClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wave|Spawn", meta=(ClampMin="0.0"))
	float SpawnDelay = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wave|Spawn")
	FVector SpawnOffset = FVector(150.f, 0.f, 0.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wave|Ground Trace", meta=(ClampMin="0.0"))
	float GroundTraceUpDistance = 500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wave|Ground Trace", meta=(ClampMin="0.0"))
	float GroundTraceDownDistance = 1500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wave|Ground Trace")
	float GroundOffset = 0.f;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	FTimerHandle SpawnTimerHandle;
	bool bWaveSpawned = false;
	bool bMontageFinished = false;
};
