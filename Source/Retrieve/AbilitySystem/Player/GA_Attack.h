#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GA_Attack.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UAbilityTask_WaitInputPress;
class UGameplayEffect;
class UWeaponComponent;

UCLASS()
class RETRIEVE_API UGA_Attack : public URetrieveGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Attack();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;

private:
	void StartComboStep(int32 StepIndex);
	void StartListeningComboInput();
	void StopRuntimeTasks();
	void CleanupComboTag() const;
	void ApplyStepDamage();

	UFUNCTION() void HandleImpactBeginEvent(FGameplayEventData Payload);
	UFUNCTION() void HandleImpactEvent(FGameplayEventData Payload);
	UFUNCTION() void HandleInputPressed(float TimeWaited);
	UFUNCTION() void HandleMontageCompleted();
	UFUNCTION() void HandleMontageInterrupted();
	UFUNCTION() void HandleMontageCancelled();

private:
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Attack")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Attack", meta = (ClampMin = "0.0"))
	float TraceRadius = 60.f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Attack")
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
	TObjectPtr<UAbilityTask_WaitInputPress> InputPressTask;

	UPROPERTY(Transient)
	TSet<TObjectPtr<AActor>> HitActorsThisStep;
	
	int32 CurrentComboIndex = INDEX_NONE;
	bool bComboQueued = false;
	
	FVector PreviousTraceOrigin = FVector::ZeroVector;
	bool bHasValidPreviousTraceOrigin = false;
};
