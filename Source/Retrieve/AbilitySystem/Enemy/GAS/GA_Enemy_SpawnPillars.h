#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Enemy/GAS/GA_EnemyPatternAbilityBase.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GA_Enemy_SpawnPillars.generated.h"

class AEnemyPillar;
class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;

USTRUCT(BlueprintType)
struct RETRIEVE_API FEnemyPillarSpawnEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pillar", meta=(ClampMin="0.0"))
	float SpawnDelay = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pillar")
	FVector SpawnOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pillar")
	bool bRandomSpawn = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pillar",
		meta=(ClampMin="0.0", EditCondition="bRandomSpawn", EditConditionHides))
	float SpawnableRadius = 0.f;
};

UCLASS(Blueprintable, BlueprintType)
class RETRIEVE_API UGA_Enemy_SpawnPillars : public UGA_EnemyPatternAbilityBase
{
	GENERATED_BODY()

public:
	UGA_Enemy_SpawnPillars(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

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
	bool CacheTargetLocation(const FGameplayEventData* TriggerEventData);
	void SchedulePillars();
	void SpawnPillar(int32 SpawnEntryIndex);
	bool ResolveGroundLocation(const FVector& RequestedLocation, FVector& OutGroundLocation) const;
	void TryFinishAbility();
	void FinishAbility(bool bWasCancelled);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pillar", meta=(ClampMin="0.01"))
	float MontagePlayRate = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pillar|Spawn")
	TSubclassOf<AEnemyPillar> PillarClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pillar|Spawn")
	TArray<FEnemyPillarSpawnEntry> SpawnEntries;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pillar|Ground Trace", meta=(ClampMin="0.0"))
	float GroundTraceUpDistance = 500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pillar|Ground Trace", meta=(ClampMin="0.0"))
	float GroundTraceDownDistance = 1500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Pillar|Ground Trace")
	float GroundOffset = 0.f;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<AActor> CachedTargetActor;

	FVector CachedTargetLocation = FVector::ZeroVector;
	FMonsterLaunchKnockbackConfig ActiveLaunchKnockbackConfig;
	TArray<FTimerHandle> SpawnTimerHandles;
	int32 PendingSpawnCount = 0;
	bool bHasCachedTargetLocation = false;
	bool bMontageFinished = false;
};
