#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "GameplayTagContainer.h"
#include "GA_Absorb.generated.h"

class UGameplayEffect;
class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;

UCLASS()
class RETRIEVE_API UGA_Absorb : public URetrieveGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Absorb();

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

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

public:
	/** 원소 태그 → 적용할 GE */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Absorb")
	TMap<FGameplayTag, TSubclassOf<UGameplayEffect>> ElementToAbsorbEffect;

	/** 원소 태그 → 버프 바에 표시할 UI.Buff.Absorb.* 태그
	 *  비어 있는 원소는 버프 UI를 표시하지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Absorb")
	TMap<FGameplayTag, FGameplayTag> ElementToAbsorbBuffUITag;

	/** Legacy fallback: 장착 무기의 AttackDefinition에 Absorb 몽타주가 없을 때만 사용한다.
	 *  신규 무기별/원소별 흡수 몽타주는 DA_AttackDefinition_*의 Absorb 섹션에 넣는다. */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Absorb|Motion")
	TMap<FGameplayTag, TSoftObjectPtr<UAnimMontage>> ElementToCastMontage;

	/** Legacy fallback 몽타주 재생 속도. DA 기반 몽타주는 FWeaponAbsorbCast::MontagePlayRate를 사용한다. */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Absorb|Motion", meta = (ClampMin = "0.1"))
	float CastMontagePlayRate = 1.0f;

private:
	TSubclassOf<UGameplayEffect> ResolveAbsorbEffectClass(FGameplayTag Element) const;

	void CleanupAbsorb();
	bool PlayCastMontage(const TSoftObjectPtr<UAnimMontage>& MontagePtr, float PlayRate);
	bool PlayLegacyCastMontage(FGameplayTag Element);

	UFUNCTION() void HandleCastMontageCompleted();
	UFUNCTION() void HandleCastMontageInterrupted();
	UFUNCTION() void HandleCastMontageCancelled();

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;
};
