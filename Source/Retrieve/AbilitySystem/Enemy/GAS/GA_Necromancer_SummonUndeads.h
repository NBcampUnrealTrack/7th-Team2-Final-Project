
#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "GA_EnemyPatternAbilityBase.h"
#include "GA_Necromancer_SummonUndeads.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;
class APawn;

UCLASS(Blueprintable, BlueprintType)
class RETRIEVE_API UGA_Necromancer_SummonUndeads : public UGA_EnemyPatternAbilityBase
{
	GENERATED_BODY()

public:
	UGA_Necromancer_SummonUndeads(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

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
	// 파라미터
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Necromancer|Summon")
	TSubclassOf<APawn> FirstMinionClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Necromancer|Summon")
	TSubclassOf<APawn> SecondMinionClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Necromancer|Summon", meta = (ClampMin = "1"))
	int32 MaxMinionCount = 10;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Necromancer|Summon", meta = (ClampMin = "0.0"))
	float SummonDelay = 0.45f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Necromancer|Summon", meta = (ClampMin = "0.0"))
	float SpawnForwardDistance = 220.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Necromancer|Summon", meta = (ClampMin = "0.0"))
	float SpawnSideDistance = 140.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Necromancer|Summon", meta = (ClampMin = "0.01"))
	float MontagePlayRate = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Necromancer|Summon|Navigation")
	bool bProjectSpawnToNavigation = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Necromancer|Summon|Navigation")
	FVector NavProjectionExtent = FVector(250.f, 250.f, 300.f);

protected:
	virtual void OnMontageCompleted() override;
	virtual void OnMontageInterrupted() override;

private:
	UAnimMontage* ResolveSummonMontage(const FGameplayEventData* TriggerEventData) const;
	void ExecuteSummon();
	void CompactSpawnedUndeads();
	APawn* SpawnMinion(TSubclassOf<APawn> MinionClass, float SideOffset);
	void FinishAbility(bool bWasCancelled);
	void TryFinishAbility();

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<APawn>> SpawnedUndeads;

	FTimerHandle SummonTimerHandle;

	bool bSummonExecuted = false;
	bool bMontageFinished = false;
};
