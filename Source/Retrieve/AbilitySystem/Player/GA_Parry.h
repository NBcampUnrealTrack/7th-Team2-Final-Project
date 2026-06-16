#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Player/GA_ParryBase.h"
#include "GA_Parry.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;

/** 기본 패링 — 전 무기 공통 방어(R). 타이밍 패리(홀드 아님), 자동 종료. 방패(SwordShield)는 GA_Guard가 대신. */
UCLASS()
class RETRIEVE_API UGA_Parry : public UGA_ParryBase
{
	GENERATED_BODY()

public:
	UGA_Parry();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	UFUNCTION() void HandleParryEnd();

private:
	UPROPERTY(EditDefaultsOnly, Category = "Parry")
	TSoftObjectPtr<UAnimMontage> ParryMontage;
	
	UPROPERTY(EditDefaultsOnly, Category = "Parry", meta = (ClampMin = "0.05"))
	float ParryActiveDuration = 0.5f;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	FTimerHandle EndTimerHandle;
};
