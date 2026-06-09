#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "GA_Burst.generated.h"

struct FSkillCombination;
class UAbilityTask_PlayMontageAndWait;
class UPlayerBurstComponent;

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
	// 테이블에서 매칭 조합 반환
	const FSkillCombination* FindMatchingCombination(const TMap<FGameplayTag, int32>& ElementPattern) const;
	// 조합 매칭 체크
	static bool DoesCombinationMatch(
		const TMap<FGameplayTag, int32>& TablePattern,
		const TMap<FGameplayTag, int32>& CurrentPattern);

	UFUNCTION() void HandleMontageCompleted();
	UFUNCTION() void HandleMontageBlendOut();
	UFUNCTION() void HandleMontageInterrupted();
	UFUNCTION() void HandleMontageCancelled();

	// MontageTask 종료 + BurstComponent 정리. EndAbility/CancelAbility 공용.
	void CleanupBurst();

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
};
