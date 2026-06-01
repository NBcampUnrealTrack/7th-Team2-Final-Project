#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "GA_Dash.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;
/**
 * 
 */
UCLASS()
class RETRIEVE_API UGA_Dash : public URetrieveGameplayAbility
{
	GENERATED_BODY()
	
public:
	UGA_Dash();

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	FVector ResolveDashDirection(const FGameplayAbilityActorInfo* ActorInfo) const;

	UFUNCTION() void HandleMontageFinished();

private:
	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	TSoftObjectPtr<UAnimMontage> DashMontage;

	/** Dash 몽타주 기본 재생 속도. 1.0=ALS 원본. 최종 PlayRate = Base × (MoveSpeed / ReferenceMoveSpeed). */
	UPROPERTY(EditDefaultsOnly, Category = "Dash", meta = (ClampMin = "0.1"))
	float BaseDashPlayRate = 1.35f;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;
};
