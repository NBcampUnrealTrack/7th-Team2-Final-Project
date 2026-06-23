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
	// 게이지 슬롯의 최다 원소를 산출, 우세 없음/동률/무속성이면 Element.None(무속성)
	static FGameplayTag ResolveDominantElement(const TMap<FGameplayTag, int32>& ElementPattern);
	// 무기 타입 + 대표 원소(BurstElement)가 모두 일치하는 버스트 행 반환
	const FSkillCombination* FindBurstForElement(const FGameplayTag& WeaponType, const FGameplayTag& Element) const;

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
