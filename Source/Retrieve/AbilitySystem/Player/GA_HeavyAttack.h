#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameplayTagContainer.h"
#include "GA_HeavyAttack.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;
class UWeaponComponent;
class UPlayerBurstComponent;

/**
 * 통합 강공격 — 현재 원소의 Heavy variant를 무기 공격 자산(UWeaponAttackDefinition)에서 resolve해 타입별 실행
 */
UCLASS()
class RETRIEVE_API UGA_HeavyAttack : public URetrieveGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_HeavyAttack();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	// 근접: 공용 실행기 경로 (BeginAttackExecution)
	void RunExecutorPath();
	// 원거리: 투사체 경로
	void RunProjectilePath();
	void ScheduleProjectiles();
	void SpawnProjectile();
	AActor* ResolveAimTarget() const;

	void PlayMontageThenEnd();
	UFUNCTION() void HandleMontageFinished();

	void ApplyCastLockTags();
	void RemoveCastLockTags();
	void ExecuteOwnerCue(const FGameplayTag& CueTag) const;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Heavy|Aim", meta = (ClampMin = "0.0"))
	float AimSearchRange = 2500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Heavy|Aim", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float AimSearchHalfAngle = 35.f;

	UPROPERTY(EditDefaultsOnly, Category = "Heavy|Aim", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AimRangeWeightRate = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Heavy|Aim", meta = (ClampMin = "0.0"))
	float AimMaxVerticalDelta = 800.f;

	UPROPERTY(Transient)
	TObjectPtr<UWeaponComponent> CachedWeaponComponent;

	UPROPERTY(Transient)
	TObjectPtr<UPlayerBurstComponent> CachedBurstComp;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<AActor> CachedAimTarget;
	
	UPROPERTY(Transient)
	FWeaponHeavyAttack CachedVariant;

	FGameplayTag CachedElementTag;
	FGameplayTagContainer AppliedCastLockTags;
	TArray<FTimerHandle> SpawnTimerHandles;
	bool bUsedBurstExecutor = false;
};
