#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "GA_ShootProjectileSingle.generated.h"

class AEnemyProjectile;
class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;

UCLASS(Blueprintable, BlueprintType)
class RETRIEVE_API UGA_ShootProjectileSingle : public URetrieveGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_ShootProjectileSingle(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ShootProjectileSingle")
	TSubclassOf<AEnemyProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ShootProjectileSingle")
	TObjectPtr<UAnimMontage> DefaultMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ShootProjectileSingle", meta=(ClampMin="0.0"))
	float ProjectileSpawnDelay = 0.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ShootProjectileSingle", meta=(ClampMin="0.0"))
	float ProjectileSpeed = 1200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ShootProjectileSingle")
	FName SpawnSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ShootProjectileSingle")
	FVector SpawnOffset = FVector(120.f, 0.f, 80.f);

private:
	void SpawnProjectile();
	void FinishAbility();
	const UAnimMontage* ResolveMontage(const FGameplayEventData* TriggerEventData) const;

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageInterrupted();

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<AActor> CachedTargetActor;

	FTimerHandle SpawnTimerHandle;
	FTimerHandle FinishTimerHandle;
};
