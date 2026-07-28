
#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GA_BossQueen_SwordBarrage.generated.h"

class AQueenSwordProjectile;
class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;

UCLASS(Blueprintable, BlueprintType)
class RETRIEVE_API UGA_BossQueen_SwordBarrage : public URetrieveGameplayAbility
{
	GENERATED_BODY()
	
public:
    UGA_BossQueen_SwordBarrage(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

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
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sword Barrage")
    TSubclassOf<AQueenSwordProjectile> ProjectileClass;
	bool ResolveBarrageConfig();
	FMonsterSwordBarrageConfig ActiveConfig;
    
private:
	void HandleSummon();
	void SpawnSwordArc();
	void LaunchPreparedSwords();
	void CleanupPreparedSwords();
	void TryFinishAbility();
	void FinishAbility(bool bWasCancelled);
    
	UAnimMontage* ResolveMontage(const FGameplayEventData* TriggerEventData) const;
    
	FVector ResolveTargetLocation() const;
    
	UFUNCTION()
	void OnMontageCompleted();
    
	UFUNCTION()
	void OnMontageInterrupted();
    UPROPERTY(Transient)
    TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

    UPROPERTY(Transient)
    TObjectPtr<AActor> CachedTargetActor;

    UPROPERTY(Transient)
    TArray<TObjectPtr<AQueenSwordProjectile>> PreparedSwords;

    FTimerHandle SummonTimerHandle;
    FTimerHandle LaunchTimerHandle;

    bool bMontageFinished = false;
    bool bLaunchFinished = false;
};
