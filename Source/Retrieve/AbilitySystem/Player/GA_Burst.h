#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GA_Burst.generated.h"

struct FSkillCombination;
class ACharacter;
class UAbilityTask_PlayMontageAndWait;
class UPlayerBurstComponent;
class UNiagaraSystem;

/**
 *
 */
UCLASS()
class RETRIEVE_API UGA_Burst : public URetrieveGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Burst();

protected:
	// 어빌리티 부여 시점에 모든 버스트 몽타주를 미리 로드해 발동 시 히치를 방지한다.
	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilitySpec& Spec) override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateCancelAbility) override;

private:
	// 무기 타입 + 현재 원소모드(BurstElement)가 모두 일치하는 버스트 행 반환
	const FSkillCombination* FindBurstForElement(const FGameplayTag& WeaponType, const FGameplayTag& Element) const;

	UFUNCTION() void HandleMontageCompleted();
	UFUNCTION() void HandleMontageBlendOut();
	UFUNCTION() void HandleMontageInterrupted();
	UFUNCTION() void HandleMontageCancelled();
	UFUNCTION() void HandleLanded(const FHitResult& Hit);

	// MontageTask 종료 + BurstComponent 정리. EndAbility/CancelAbility 공용.
	void CleanupBurst();
	// LandedDelegate 구독 해제
	void UnbindLanded();
	// 실제 착지 순간 발밑에 착지 VFX 스폰(설정 시).
	void SpawnLandingVFX();

	bool ShouldWaitForDiveLanding() const;
	// 다이브 착지 대기가 필요하면 대기 진입, 아니면 즉시 EndAbility.
	void FinishOrWaitForLanding();

	// 스킬 타입별 시전 잠금(이동/회전) 적용/해제
	void ApplyCastLockTags(const FSkillCombination* Combo);
	void RemoveCastLockTags();

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill")
	TObjectPtr<UDataTable> SkillCombinationTable;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UPlayerBurstComponent> CachedBurstComp;
	
	FGameplayTagContainer AppliedCastLockTags;

	// ---- Landing Impact (bDoLandingImpact 버스트: 낙법/래그돌 억제 + 착지섹션 점프 + AoE/넉백) ----
	UPROPERTY(Transient)
	TWeakObjectPtr<ACharacter> BoundLandedCharacter;
	bool bLandingImpactEnabled = false;
	bool bLandingHandled = false;
	FName CachedLandingSection = NAME_None;
	float CachedLandingRadius = 0.f;
	float CachedLandingDamageMul = 1.f;
	bool bCachedUseLandingKnockback = false;
	bool bCachedExcludeBoss = true;
	FRetrieveKnockbackParams CachedLandingKnockback;
	// 실제 착지 순간 발밑에 재생할 VFX(몽타주 노티 대신 물리 착지에 맞춰 스폰).
	TSoftObjectPtr<UNiagaraSystem> CachedLandingVFX;
	float CachedLandingVFXScale = 1.f;

	// 몽타주가 다이브 착지 전에 끝났을 때, 실제 착지까지 어빌리티를 살려두는 대기 상태.
	bool bWaitingForLanding = false;
	// 착지 이벤트가 끝내 오지 않을 때(낭떠러지 낙하 + 루프 몽타주) 강제 종료하는 안전 타임아웃 핸들.
	FTimerHandle LandingWaitTimeoutHandle;
	// 시전 시점부터 착지까지 허용하는 최대 대기(초). 넘으면 강제 종료(무한 대기 방지).
	static constexpr float LandingSafetyTimeout = 8.f;
};
