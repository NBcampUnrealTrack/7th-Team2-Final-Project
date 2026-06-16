#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GA_JumpAttack.generated.h"

class ACharacter;
class UAbilityTask_PlayMontageAndWait;
class UGameplayEffect;
class UWeaponComponent;

/**
 * 공중(점프) 상태에서 발동하는 찍기(슬램) 공격으로, 데미지는 착지 시점에만 발생
 * 발동 시 DiveGravityScale로 급강하하며, 발동 시점의 지면 높이에 따라 피해를 차등 적용
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
	void UnbindLanded();
	void ApplyLandingAoe();
	void ResolveHeightTier();
	void RestoreGravityScale();	
	
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

	// 발동 시 해결된 Jump variant 값 복사본 (원소별 → 없으면 기본)
	UPROPERTY(Transient)
	FWeaponJumpAttack CachedJumpData;

	UPROPERTY(Transient)
	TObjectPtr<UWeaponComponent> CachedWeaponComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TSet<TObjectPtr<AActor>> HitActors;

	bool bChargeBonusGranted = false;
	
	float ResolvedDamageMultiplier = 1.f;
	ERetrieveHitReactType ResolvedHitReactType = ERetrieveHitReactType::Flinch;
	float ResolvedAoeRadius = 0.f;
	
	float SavedGravityScale = 1.f;
	bool bGravityModified = false;
	bool bLandingHandled = false;
	
	// LandedDelegate 구독 해제용
	UPROPERTY(Transient)
	TWeakObjectPtr<ACharacter> BoundLandedCharacter;
};
