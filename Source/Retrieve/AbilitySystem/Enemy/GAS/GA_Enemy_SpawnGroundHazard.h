#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Enemy/GAS/GA_EnemyPatternAbilityBase.h"
#include "GA_Enemy_SpawnGroundHazard.generated.h"

class AEnemyGroundHazard;
class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;

USTRUCT(BlueprintType)
struct RETRIEVE_API FEnemyGroundHazardSpawnEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ground Hazard", meta=(ClampMin="0.0"))
	float SpawnDelay = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ground Hazard")
	FVector SpawnOffset = FVector::ZeroVector;
};

UCLASS(Blueprintable, BlueprintType)
class RETRIEVE_API UGA_Enemy_SpawnGroundHazard : public UGA_EnemyPatternAbilityBase
{
	GENERATED_BODY()

public:
	UGA_Enemy_SpawnGroundHazard(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

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
	UAnimMontage* ResolveAttackMontage(const FGameplayEventData* TriggerEventData) const;
	void ScheduleGroundHazards();
	void SpawnGroundHazard(int32 SpawnEntryIndex);
	bool ResolveGroundLocation(const FVector& RequestedLocation, FVector& OutGroundLocation) const;
	bool IsTooCloseToExistingHazard(const FVector& GroundLocation) const;
	void TryFinishAbility();
	void FinishAbility(bool bWasCancelled);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ground Hazard", meta=(ClampMin="0.01"))
	float MontagePlayRate = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ground Hazard|Spawn")
	TSubclassOf<AEnemyGroundHazard> GroundHazardClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ground Hazard|Spawn")
	FName SpawnBoneName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ground Hazard|Spawn")
	TArray<FEnemyGroundHazardSpawnEntry> SpawnEntries;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ground Hazard|Spawn")
	bool bRandomSpawn = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ground Hazard|Spawn",
		meta=(ClampMin="0.0", EditCondition="bRandomSpawn", EditConditionHides))
	float SpawnableRadius = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ground Hazard|Ground Trace", meta=(ClampMin="0.0"))
	float GroundTraceUpDistance = 200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ground Hazard|Ground Trace", meta=(ClampMin="0.0"))
	float GroundTraceDownDistance = 1000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ground Hazard|Ground Trace")
	float GroundOffset = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ground Hazard|Ground Trace", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MinGroundNormalZ = 0.7f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ground Hazard|Placement", meta=(ClampMin="0.0"))
	float MinimumSpawnSpacing = 200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Ground Hazard|Placement", meta=(ClampMin="1"))
	int32 MaximumPlacementAttempts = 30;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	TArray<FTimerHandle> SpawnTimerHandles;
	TArray<FVector> SpawnedGroundLocations;
	int32 PendingSpawnCount = 0;
	bool bMontageFinished = false;
};
