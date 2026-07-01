#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Enemy/GAS/GA_EnemyPatternAbilityBase.h"
#include "GA_Enemy_SpawnTornadoHazard.generated.h"

class AEnemyTornadoHazard;
class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;

USTRUCT(BlueprintType)
struct RETRIEVE_API FEnemyTornadoHazardSpawnEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tornado", meta=(ClampMin="0.0"))
	float SpawnDelay = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tornado")
	FVector SpawnOffset = FVector::ZeroVector;
};

UCLASS(Blueprintable, BlueprintType)
class RETRIEVE_API UGA_Enemy_SpawnTornadoHazard : public UGA_EnemyPatternAbilityBase
{
	GENERATED_BODY()

public:
	UGA_Enemy_SpawnTornadoHazard(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

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
	void ScheduleTornadoSpawns();
	void SpawnTornado(int32 SpawnEntryIndex);
	bool ResolveGroundLocation(const FVector& RequestedLocation, FVector& OutGroundLocation) const;
	void TryFinishAbility();
	void FinishAbility(bool bWasCancelled);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tornado", meta=(ClampMin="0.01"))
	float MontagePlayRate = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tornado|Spawn")
	TSubclassOf<AEnemyTornadoHazard> TornadoHazardClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tornado|Spawn")
	TArray<FEnemyTornadoHazardSpawnEntry> SpawnEntries;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tornado|Ground Trace", meta=(ClampMin="0.0"))
	float GroundTraceUpDistance = 500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tornado|Ground Trace", meta=(ClampMin="0.0"))
	float GroundTraceDownDistance = 1500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Tornado|Ground Trace")
	float GroundOffset = 0.f;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	TArray<FTimerHandle> SpawnTimerHandles;
	int32 PendingSpawnCount = 0;
	bool bMontageFinished = false;
};
