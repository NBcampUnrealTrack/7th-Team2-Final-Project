#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GA_SprintAttack.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UGameplayEffect;
class UWeaponComponent;

/**
 * 스프린트 중 공격 입력 시 발동하는 단발 찌르기 공격
 */
UCLASS()
class RETRIEVE_API UGA_SprintAttack : public URetrieveGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_SprintAttack();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;

private:
	void StopRuntimeTasks();
	void ApplyHitDamage();
	void BuildTracePoints(TArray<FVector>& OutPoints) const;

	UFUNCTION() void HandleImpactBeginEvent(FGameplayEventData Payload);
	UFUNCTION() void HandleImpactEvent(FGameplayEventData Payload);
	UFUNCTION() void HandleMontageCompleted();
	UFUNCTION() void HandleMontageInterrupted();
	UFUNCTION() void HandleMontageCancelled();

private:
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|SprintAttack")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|SprintAttack")
	bool bDebugDrawTrace = false;

	UPROPERTY(Transient)
	FRetrieveWeaponDataRow CachedWeaponData;

	UPROPERTY(Transient)
	TObjectPtr<UWeaponComponent> CachedWeaponComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ImpactBeginEventTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ImpactEventTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TSet<TObjectPtr<AActor>> HitActors;

	bool bChargeBonusGranted = false;

	TArray<FVector> PreviousTracePoints;
	bool bHasValidPreviousTracePoints = false;
};
