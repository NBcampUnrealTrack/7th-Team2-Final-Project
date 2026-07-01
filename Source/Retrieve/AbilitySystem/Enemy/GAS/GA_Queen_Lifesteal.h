
#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "GA_Queen_Lifesteal.generated.h"

class UAbilityTask_WaitGameplayEvent;
class UGameplayEffect;
struct FGameplayEventData;

UCLASS(Blueprintable, BlueprintType)
class RETRIEVE_API UGA_Queen_Lifesteal : public URetrieveGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_Queen_Lifesteal(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

protected:
	// 가한 데미지 대비 흡혈 비율
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lifesteal", meta = (ClampMin = "0.0"))
	float LifestealRatio = 0.4f;

	// 흡혈이 켜지는 시작 페이즈 (이 페이즈 이상부터)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lifesteal", meta = (ClampMin = "1"))
	int32 MinPhase = 2;

	// IncomingHealing SetByCaller GE = GE_Boss_Lifesteal
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lifesteal")
	TSubclassOf<UGameplayEffect> LifestealHealEffect;

private:
	UFUNCTION()
	void OnQueenDealtDamage(FGameplayEventData Payload);

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> WaitHitTask;
};
