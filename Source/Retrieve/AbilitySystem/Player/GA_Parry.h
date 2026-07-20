#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Player/GA_ParryBase.h"
#include "GA_Parry.generated.h"

class UAbilityTask_PlayMontageAndWait;

/**
 * 기본 패링 — 비방패 무기 공통 방어(R). 타이밍 패리(홀드 아님).
 * 패링 시도 몽타주에 배치된 AnimNotifyState_ParryWindow가 판정 창을 연다. 방패(SwordShield)는 GA_Guard가 담당.
 */
UCLASS()
class RETRIEVE_API UGA_Parry : public UGA_ParryBase
{
	GENERATED_BODY()

public:
	UGA_Parry();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	// AnimNotifyState_ParryWindow 훅: 시도 몽타주의 창 구간에만 패링 판정을 켠다.
	virtual bool OpenNotifyParryWindow() override;
	virtual void CloseNotifyParryWindow() override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	UFUNCTION() void HandleMontageFinished();

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;
};
