#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GA_ParryCounter.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UGameplayEffect;
class UWeaponComponent;

/**
 * 패리 성공 후 발동하는 카운터 어빌리티
 */
UCLASS()
class RETRIEVE_API UGA_ParryCounter : public URetrieveGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_ParryCounter();

	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr, FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	virtual void CancelAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateCancelAbility) override;

private:
	void StopRuntimeTasks();

	AActor* ResolveCounterTarget() const;

	void ApplyCounterToTarget(AActor* TargetActor);

	void ApplyGroggyToTarget(AActor* TargetActor, UAbilitySystemComponent* TargetASC) const;

	UFUNCTION() void HandleMontageCompleted();
	UFUNCTION() void HandleMontageInterrupted();
	UFUNCTION() void HandleMontageCancelled();

private:
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|ParryCounter")
	TSubclassOf<UGameplayEffect> DamageEffectClass;
	
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|ParryCounter")
	FGameplayTag GroggyDurationTag;

	UPROPERTY(Transient)
	FRetrieveWeaponDataRow CachedWeaponData;

	// 발동 시 해결된 ParryCounter variant 값 복사본 (원소별 → 없으면 기본)
	UPROPERTY(Transient)
	FParryCounterData CachedParryData;

	UPROPERTY(Transient)
	TObjectPtr<UWeaponComponent> CachedWeaponComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;
};
