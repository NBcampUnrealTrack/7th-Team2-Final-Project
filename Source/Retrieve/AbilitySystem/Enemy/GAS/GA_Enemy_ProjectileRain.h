#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Enemy/GAS/GA_EnemyPatternAbilityBase.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GA_Enemy_ProjectileRain.generated.h"

class AEnemyProjectile;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAnimMontage;
class UDecalComponent;
class UGameplayEffect;
class UMaterialInterface;

UCLASS(Blueprintable, BlueprintType)
class RETRIEVE_API UGA_Enemy_ProjectileRain : public UGA_EnemyPatternAbilityBase
{
	GENERATED_BODY()

public:
	UGA_Enemy_ProjectileRain(
		const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

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
	bool BuildSpawnLocations();
	bool ResolveGroundLocation(const FVector& RequestedLocation, FVector& OutGroundLocation) const;
	bool ResolveProjectileSpawnLocation(const FVector& GroundLocation, FVector& OutSpawnLocation) const;
	void SchedulePreparedProjectiles();
	void SpawnPreparedProjectile(int32 SpawnPointIndex);
	void ReleasePreparedProjectiles();
	void FadeWarningDecals();
	void FinishAbility(bool bWasCancelled);

	UFUNCTION()
	void HandleReleaseEvent(FGameplayEventData Payload);

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain", meta=(ClampMin="0.01"))
	float MontagePlayRate = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain|Spawn")
	TSubclassOf<AEnemyProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain|Placement", meta=(ClampMin="0.0"))
	float WarningRadius = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain|Placement", meta=(ClampMin="0.0"))
	float SpacingPadding = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain|Placement", meta=(ClampMin="1"))
	int32 MaximumPlacementAttempts = 100;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain|Ground Trace", meta=(ClampMin="0.0"))
	float GroundTraceUpDistance = 500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain|Ground Trace", meta=(ClampMin="0.0"))
	float GroundTraceDownDistance = 1500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain|Ground Trace")
	float GroundOffset = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain|Ceiling Trace", meta=(ClampMin="0.0"))
	float CeilingTraceExtraDistance = 300.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain|Ceiling Trace", meta=(ClampMin="0.0"))
	float CeilingClearance = 50.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain|Warning")
	TObjectPtr<UMaterialInterface> WarningDecalMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain|Warning", meta=(ClampMin="0.0"))
	float WarningDecalDepth = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain|Warning", meta=(ClampMin="0.0"))
	float WarningDecalFadeOutDuration = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rain|Release")
	FGameplayTag ReleaseEventTag;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ReleaseEventTask;

	UPROPERTY(Transient)
	TObjectPtr<AActor> CachedTargetActor;

	FVector CachedTargetLocation = FVector::ZeroVector;
	TArray<FVector> GroundLocations;
	TArray<FVector> ProjectileSpawnLocations;
	TArray<TWeakObjectPtr<AEnemyProjectile>> PreparedProjectiles;
	TArray<TWeakObjectPtr<UDecalComponent>> WarningDecals;
	TArray<FTimerHandle> PreparationTimerHandles;
	TSet<int32> PreparedSpawnPointIndices;
	FMonsterProjectilePatternConfig ActiveProjectileConfig;
	FMonsterLaunchKnockbackConfig ActiveLaunchKnockbackConfig;
	ERetrieveHitReactType ActiveHitReactType = ERetrieveHitReactType::Flinch;
	FGameplayTag ActiveEffectTag;
	TSubclassOf<UGameplayEffect> ActiveStatusEffectClass;
	TSubclassOf<AEnemyProjectile> ActiveProjectileClass;
	bool bProjectilesReleased = false;
};
