#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Player/GA_HeavyAttack_Base.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GA_StaffHeavyAttack.generated.h"

class UWeaponComponent;
class AStaffProjectile;

/**
 * 배틀메이지 강공격 — 왼손에서 원소 투사체 발사
 */
UCLASS()
class RETRIEVE_API UGA_StaffHeavyAttack : public UGA_HeavyAttack_Base
{
	GENERATED_BODY()

public:
	UGA_StaffHeavyAttack();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void ExecuteHeavyEffect(const FGameplayTag& ConsumedElement) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	void ScheduleProjectiles();
	void SpawnProjectile();

	// 조준: 락온 우선 → 없으면 전방 콘 검색
	AActor* ResolveAimTarget() const;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Staff|Aim", meta = (ClampMin = "0.0"))
	float AimSearchRange = 2500.f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Staff|Aim", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float AimSearchHalfAngle = 35.f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Staff|Aim", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AimRangeWeightRate = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Staff|Aim", meta = (ClampMin = "0.0"))
	float AimMaxVerticalDelta = 800.f;

	UPROPERTY(Transient)
	FRetrieveWeaponDataRow CachedWeaponData;

	UPROPERTY(Transient)
	TObjectPtr<UWeaponComponent> CachedWeaponComponent;

	UPROPERTY(Transient)
	TObjectPtr<AActor> CachedAimTarget;

	TArray<FTimerHandle> SpawnTimerHandles;

	FGameplayTag CachedElementTag;
};
