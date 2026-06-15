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
	bool ResolveAttackComboVariant();

	// 워프 타겟 해석: 락온 우선 → 없으면 전방 콘 검색(URetrieveTargetingLibrary)
	AActor* ResolveAttackWarpTarget() const;
	// 현재 타겟 기준 WarpTarget 등록. 타겟 없으면 RemoveWarpTarget(루트모션 유지)
	void RegisterAttackWarpTarget();
	
	void BuildTracePoints(TArray<FVector>& OutPoints) const;

	/** 콤보 몽타주 재생 속도. CombatAttributeSet의 AttackSpeedMultiplier를 사용(원소 각성 버프 등 반영). */
	float GetMontagePlayRate() const;

	UFUNCTION() void HandleImpactBeginEvent(FGameplayEventData Payload);
	UFUNCTION() void HandleImpactEvent(FGameplayEventData Payload);
	UFUNCTION() void HandleInputPressed(float TimeWaited);
	UFUNCTION() void HandleMontageCompleted();
	UFUNCTION() void HandleMontageInterrupted();
	UFUNCTION() void HandleMontageCancelled();
	UFUNCTION() void HandleMontageBlendOut();

private:
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Attack")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Attack")
	bool bDebugDrawTrace = false;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Attack|Warp", meta = (ClampMin = "0.0"))
	float WarpSearchRange = 350.f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Attack|Warp", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float WarpSearchHalfAngle = 60.f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Attack|Warp", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float WarpRangeWeightRate = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Attack|Warp", meta = (ClampMin = "0.0"))
	float WarpStandoffOffset = 20.f;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Attack|Warp", meta = (ClampMin = "0.0"))
	float WarpMaxVerticalDelta = 120.f;

	// 몽타주 Motion Warping Notify의 "Warp Target Name"과 반드시 동일해야 함
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Attack|Warp")
	FName AttackWarpTargetName = TEXT("AttackTarget");

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
	TObjectPtr<UAbilityTask_WaitInputPress> InputPressTask;

	UPROPERTY(Transient)
	TSet<TObjectPtr<AActor>> HitActorsThisStep;
	
	int32 PendingComboIndex = INDEX_NONE;
	int32 CurrentComboIndex = INDEX_NONE;

	FGameplayTag CachedElementTag;
	bool bPendingElementRestart = false;
	bool bComboChargeBonusGranted = false;
	
	TArray<FVector> PreviousTracePoints;
	bool bHasValidPreviousTracePoints = false;
};
