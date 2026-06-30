#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GA_Attack.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UGameplayEffect;
class UWeaponComponent;
struct FRetrieveBufferedCombatInput;

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

	// 활성 중 버퍼된 '평타' 입력을 콤보 다음 타로 소비한다(ASC 리졸버가 호출). 원소/외부공격은 여기서 안 다룸.
	virtual bool TryConsumeBufferedCombatInput(const FRetrieveBufferedCombatInput& BufferedInput) override;

private:
	void StartComboStep(int32 StepIndex);
	void StopRuntimeTasks();
	void CleanupAttackWindowTags() const;
	void ApplyStepDamage();
	bool ResolveAttackComboVariant();

	/** 콤보 몽타주 재생 속도. CombatAttributeSet의 AttackSpeedMultiplier를 사용(원소 각성 버프 등 반영). */
	float GetMontagePlayRate() const;

	UFUNCTION() void HandleImpactBeginEvent(FGameplayEventData Payload);
	UFUNCTION() void HandleImpactEvent(FGameplayEventData Payload);
	UFUNCTION() void HandleMontageCompleted();
	UFUNCTION() void HandleMontageInterrupted();
	UFUNCTION() void HandleMontageCancelled();
	UFUNCTION() void HandleMontageBlendOut();

private:
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Attack")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Attack")
	bool bDebugDrawTrace = false;

	UPROPERTY(Transient)
	FRetrieveWeaponDataRow CachedWeaponData;
	
	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> CachedAttackMontage;
	
	UPROPERTY(Transient)
	TArray<FWeaponComboStep> CachedComboSteps;
	
	UPROPERTY(Transient) 
	TObjectPtr<UWeaponComponent> CachedWeaponComponent;
	
	UPROPERTY(Transient) 
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ImpactBeginEventTask;
	
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ImpactEventTask;
	
	UPROPERTY(Transient) 
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;
	
	UPROPERTY(Transient)
	TSet<TObjectPtr<AActor>> HitActorsThisStep;

	int32 CurrentComboIndex = INDEX_NONE;

	FGameplayTag CachedElementTag;
	bool bComboChargeBonusGranted = false;
	
	TArray<FVector> PreviousTracePoints;
	bool bHasValidPreviousTracePoints = false;
	
	TArray<TArray<FVector>> PreviousTracePointsPerPart;
};
