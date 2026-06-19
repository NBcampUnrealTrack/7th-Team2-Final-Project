#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Enemy/GAS/GA_EnemyPatternAbilityBase.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GA_ShootProjectiles.generated.h"

class AEnemyProjectile;
class ACharacter;
class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;
class UGameplayEffect;

UCLASS(Blueprintable, BlueprintType)
class RETRIEVE_API UGA_ShootProjectiles : public UGA_EnemyPatternAbilityBase
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

protected:
	virtual UAnimMontage* ResolveFallbackSequenceMontage() const { return nullptr; }
	virtual void OnSpecialAttackActivated() {}
	virtual void OnSpecialAttackEnded() {}
	virtual void OnBeforeProjectileSpawn() {}
	virtual void OnProjectileSpawned(AEnemyProjectile* Projectile, AActor* AvatarActor) {}

	AActor* GetCachedTargetActor() const { return CachedTargetActor; }
	const FMonsterProjectilePatternConfig& GetActiveProjectileConfig() const { return ActiveProjectileConfig; }

	virtual void OnMontageCompleted() override;

	virtual void OnMontageInterrupted() override;

	void ScheduleProjectiles(bool bHasMontage);
	void SpawnProjectile();
	void FinishAbility();
	TSubclassOf<AEnemyProjectile> ResolveProjectileClass() const;
	bool ResolveProjectilePattern(FMonsterProjectilePatternConfig& OutConfig,
		ERetrieveHitReactType* OutHitReactType = nullptr,
		FMonsterLaunchKnockbackConfig* OutLaunchKnockbackConfig = nullptr,
		FGameplayTag* OutEffectTag = nullptr,
		TSubclassOf<UGameplayEffect>* OutStatusEffectClass = nullptr,
		TSubclassOf<AEnemyProjectile>* OutProjectileClass = nullptr) const;
	const UAnimMontage* ResolveMontage(const FGameplayEventData* TriggerEventData) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ShootProjectiles")
	TSubclassOf<AEnemyProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ShootProjectiles", meta=(ClampMin="0.0"))
	float FallbackProjectileSpawnDelay = 0.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="ShootProjectiles", meta=(ClampMin="0.0"))
	float FallbackProjectileSpeed = 1200.f;

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
	int32 ActiveProjectileSpawnIndex = 0;
	int32 ActiveProjectileCount = 0;

	FMonsterProjectilePatternConfig ActiveProjectileConfig;
	ERetrieveHitReactType ActiveHitReactType = ERetrieveHitReactType::Flinch;
	FMonsterLaunchKnockbackConfig ActiveLaunchKnockbackConfig;
	FGameplayTag ActiveEffectTag;
	TSubclassOf<UGameplayEffect> ActiveStatusEffectClass;
	TSubclassOf<AEnemyProjectile> ActiveProjectileClass;
	FName ActivePatternRowName = NAME_None;
};
