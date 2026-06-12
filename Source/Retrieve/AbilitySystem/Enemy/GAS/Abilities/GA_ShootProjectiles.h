#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GA_ShootProjectiles.generated.h"

class AEnemyProjectile;
class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;

UCLASS(Blueprintable, BlueprintType)
class RETRIEVE_API UGA_ShootProjectiles : public URetrieveGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_ShootProjectiles(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
	bool bWasCancelled) override;

private:
	void ScheduleProjectiles(bool bHasMontage);
	void SpawnProjectile();
	void FinishAbility();
	bool ResolveProjectilePattern(FMonsterProjectilePatternConfig& OutConfig) const;
	const UAnimMontage* ResolveMontage(const FGameplayEventData* TriggerEventData) const;
	

	UFUNCTION()
	void OnMontageCompleted();

	UFUNCTION()
	void OnMontageInterrupted();
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ShootProjectiles")
	TSubclassOf<AEnemyProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ShootProjectiles", meta=(ClampMin="0.0"))
	float FallbackProjectileSpawnDelay = 0.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ShootProjectiles", meta=(ClampMin="0.0"))
	float FallbackProjectileSpeed  = 1200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ShootProjectiles")
	FName SpawnSocketName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ShootProjectiles")
	FVector SpawnOffset = FVector(120.f, 0.f, 80.f);

private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<AActor> CachedTargetActor;

	TArray<FTimerHandle> SpawnTimerHandles;
	FTimerHandle FinishTimerHandle;
	float ActiveProjectileSpeed = 1200.f;
	
	FMonsterProjectilePatternConfig ActiveProjectileConfig;
};
