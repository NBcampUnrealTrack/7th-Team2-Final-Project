#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "GA_ParryBase.generated.h"

class UAbilityTask_WaitGameplayEvent;
class UGameplayEffect;

/**
 * 패링 공용 베이스(추상) — 패링 윈도우/쿨다운 + 성공 시 카운터 윈도우·적 스태거·마지막 패링 대상 추적.
 */
UCLASS(Abstract)
class RETRIEVE_API UGA_ParryBase : public URetrieveGameplayAbility
{
	GENERATED_BODY()

public:
	AActor* GetLastParriedAttacker() const { return LastParriedAttacker.Get(); }

protected:
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	
	void OpenParryWindow();
	void StartListeningForParrySuccess();
	void StopParrySuccessTask();

	UFUNCTION()
	void HandleParrySuccess(FGameplayEventData Payload);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Parry")
	TSubclassOf<UGameplayEffect> ParryWindowEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Parry")
	TSubclassOf<UGameplayEffect> ParryCooldownEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Parry")
	TSubclassOf<UGameplayEffect> CounterWindowEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Parry")
	TSubclassOf<UGameplayEffect> ParryStaggerEffect;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> ParrySuccessTask;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> LastParriedAttacker;
};
