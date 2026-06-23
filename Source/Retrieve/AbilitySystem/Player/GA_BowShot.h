#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "Combat/RetrieveCombatTypes.h"
#include "GameplayTagContainer.h"
#include "GA_BowShot.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;
class UGameplayEffect;
class UWeaponComponent;
class AStaffProjectile;

/**
 * 활(궁수) 일반공격 — 화살 투사체 발사
 */
UCLASS()
class RETRIEVE_API UGA_BowShot : public URetrieveGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_BowShot();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	void ScheduleProjectiles();
	void SpawnProjectile();
	void PlayFireMontageThenEnd();
	
	AActor* ResolveAimTarget() const;

	UFUNCTION()
	void HandleMontageFinished();

private:
	// ---- 발사체 ----
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow")
	TSubclassOf<AStaffProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow", meta = (ClampMin = "0.0"))
	float ProjectileSpeed = 3000.f;
	
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow")
	TArray<float> FireDelays;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow")
	ERetrieveHitReactType HitReactType = ERetrieveHitReactType::Flinch;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow")
	FName SpawnSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow")
	FVector SpawnOffset = FVector(40.f, 0.f, 50.f);
	
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow")
	FGameplayTag ChargeBonusEventTag;
	
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow|Element")
	TMap<FGameplayTag, TSubclassOf<UGameplayEffect>> ElementStatusEffects;

	// ---- 시전 몽타주 ----
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow")
	TSoftObjectPtr<UAnimMontage> FireMontage;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow", meta = (ClampMin = "0.1"))
	float MontagePlayRate = 1.0f;

	// ---- 조준 ----
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow|Aim", meta = (ClampMin = "0.0"))
	float AimSearchRange = 3000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow|Aim", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float AimSearchHalfAngle = 30.f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow|Aim", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AimRangeWeightRate = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Bow|Aim", meta = (ClampMin = "0.0"))
	float AimMaxVerticalDelta = 800.f;

	UPROPERTY(Transient)
	TObjectPtr<UWeaponComponent> CachedWeaponComponent;

	UPROPERTY(Transient)
	TObjectPtr<AActor> CachedAimTarget;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	TArray<FTimerHandle> SpawnTimerHandles;

	FGameplayTag CachedElementTag;
};
