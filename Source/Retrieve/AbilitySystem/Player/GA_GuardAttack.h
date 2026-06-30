#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Player/GA_ParryBase.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GA_GuardAttack.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;
class UGameplayEffect;
class UMeshComponent;
class UWeaponComponent;

/**
 * 검방패 전용 GuardAttack.
 *
 * 역할:
 * - Guard 유지(GA_Guard)와 분리된 "방패 공격" 액션.
 * - 데미지는 일반 공격과 같은 DamageEffectClass / SetByCaller(Data.Damage.Mul) 경로를 사용한다.
 * - 패리 window는 bCanStartParry가 true인 데이터에서만 열 수 있다.
 *
 * 주의:
 * - 공격 trace window와 parry window는 서로 독립이다.
 * - AttackImpact NotifyState는 방패가 적을 때리는 데미지 판정만 담당한다.
 * - AnimNotifyState_ParryWindow는 "맞았을 때 패리 가능한 시간"만 담당한다.
 * - NotifyState는 UGA_GuardAttack을 직접 참조하지 않고, URetrieveGameplayAbility의 공용 hook을 호출한다.
 *   그래야 추후 일반 공격도 bCanStartParry만 추가해 같은 NotifyState를 재사용할 수 있다.
 */
UCLASS()
class RETRIEVE_API UGA_GuardAttack : public UGA_ParryBase
{
	GENERATED_BODY()

public:
	UGA_GuardAttack();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	// AnimNotifyState_ParryWindow가 호출하는 공용 hook.
	// GuardAttack 데이터의 bCanStartParry가 true일 때만 실제 ParryWindow를 연다.
	virtual bool OpenNotifyParryWindow() override;

	// AnimNotifyState_ParryWindow 종료 시 호출된다.
	// 성공하지 않고 window가 닫힌 경우도 "패리 시도 1회"로 보고 cooldown을 적용한다.
	virtual void CloseNotifyParryWindow() override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;

private:
	bool ResolveGuardAttackData();
	void StopRuntimeTasks();
	void ApplyHitDamage();
	void BuildTracePoints(TArray<FVector>& OutPoints) const;
	void BuildWeaponTracePoints(TArray<FVector>& OutPoints) const;
	void BuildShieldTracePoints(TArray<FVector>& OutPoints) const;
	void SweepAndApplyDamage(const TArray<FVector>& CurrentPoints);

	UFUNCTION() void HandleImpactBeginEvent(FGameplayEventData Payload);
	UFUNCTION() void HandleImpactEvent(FGameplayEventData Payload);
	UFUNCTION() void HandleMontageCompleted();
	UFUNCTION() void HandleMontageInterrupted();
	UFUNCTION() void HandleMontageCancelled();

private:
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|GuardAttack")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|GuardAttack")
	bool bDebugDrawTrace = false;

	UPROPERTY(Transient)
	FRetrieveWeaponDataRow CachedWeaponData;

	UPROPERTY(Transient)
	FWeaponGuardAttackData CachedGuardAttackData;

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

	TArray<FVector> PreviousTracePoints;
	bool bHasValidPreviousTracePoints = false;
	
	mutable float ResolvedTraceRadius = 0.f;
};
