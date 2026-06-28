#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GA_ParryCounter.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UGameplayEffect;
class UWeaponComponent;

/**
 * 패리 성공 후 발동하는 카운터 어빌리티
 */
UCLASS()
class RETRIEVE_API UGA_ParryCounter : public URetrieveGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_ParryCounter();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;

private:
	void StopRuntimeTasks();

	AActor* ResolveCounterTarget() const;
	void RegisterCounterWarpTarget();
	void ClearCounterWarpTarget();

	void ApplyCounterToTarget(AActor* TargetActor);

	bool TryApplyMonsterGroggy(AActor* TargetActor, float Duration) const;

	UFUNCTION() void HandleImpactEvent(FGameplayEventData Payload);
	UFUNCTION() void HandleMontageCompleted();
	UFUNCTION() void HandleMontageInterrupted();
	UFUNCTION() void HandleMontageCancelled();

private:
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|ParryCounter")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|ParryCounter|Damage", meta = (ClampMin = "0.0"))
	float CounterDamageMultiplier = 2.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|ParryCounter|Damage")
	ERetrieveHitReactType CounterHitReactType = ERetrieveHitReactType::Stagger;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|ParryCounter|Damage", meta = (Categories = "GameplayEvent.Attack.HitSuccess"))
	FGameplayTag CounterHitSuccessFeedbackTag;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|ParryCounter|Damage", meta = (Categories = "GameplayEvent.Hit"))
	FGameplayTag CounterTargetHitFeedbackTag;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|ParryCounter|Damage", meta = (ClampMin = "0.0"))
	float CounterKnockbackStrength = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|ParryCounter|Damage", meta = (ClampMin = "0.0"))
	float CounterKnockbackUpwardStrength = 0.f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|ParryCounter|Groggy")
	bool bApplyGroggyOnImpact = true;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|ParryCounter|Groggy", meta = (ClampMin = "0.0"))
	float CounterGroggyDuration = 3.f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|ParryCounter|MotionWarping", meta = (ClampMin = "0.0"))
	float CounterWarpStandoffOffset = 50.f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|ParryCounter|MotionWarping", meta = (ClampMin = "0.0"))
	float CounterMaxWarpDistance = 500.f;

	UPROPERTY(Transient)
	FRetrieveWeaponDataRow CachedWeaponData;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CachedCounterTarget;

	UPROPERTY(Transient)
	bool bCounterImpactApplied = false;

	UPROPERTY(Transient)
	TObjectPtr<UWeaponComponent> CachedWeaponComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ImpactEventTask;
};
