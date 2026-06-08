#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GA_JumpAttack.generated.h"

class ACharacter;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UGameplayEffect;
class UWeaponComponent;

/**
 * 공중(점프) 상태에서만 발동하는 단발 공중 공격
 */
UCLASS()
class RETRIEVE_API UGA_JumpAttack : public URetrieveGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_JumpAttack();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;

private:
	void StopRuntimeTasks();
	void ApplyHitDamage();
	void BuildTracePoints(TArray<FVector>& OutPoints) const;

	void UnbindLanded();

	UFUNCTION() void HandleImpactBeginEvent(FGameplayEventData Payload);
	UFUNCTION() void HandleImpactEvent(FGameplayEventData Payload);
	UFUNCTION() void HandleMontageCompleted();
	UFUNCTION() void HandleMontageInterrupted();
	UFUNCTION() void HandleMontageCancelled();
	UFUNCTION() void HandleLanded(const FHitResult& Hit);

private:
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|JumpAttack")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|JumpAttack")
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

	// LandedDelegate 구독 해제용
	UPROPERTY(Transient)
	TWeakObjectPtr<ACharacter> BoundLandedCharacter;
};
