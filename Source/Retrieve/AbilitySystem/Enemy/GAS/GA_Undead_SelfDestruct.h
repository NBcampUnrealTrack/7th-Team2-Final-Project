
#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "GA_EnemyPatternAbilityBase.h"
#include "GA_Undead_SelfDestruct.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;
class UGameplayEffect;

UCLASS(Blueprintable, BlueprintType)
class RETRIEVE_API UGA_Undead_SelfDestruct : public UGA_EnemyPatternAbilityBase
{
	GENERATED_BODY()

public:
	UGA_Undead_SelfDestruct(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

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
	// 파라미터
	// 이 거리 안에 들어오면 퓨즈 시작
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Chase", meta = (ClampMin = "0.0"))
	float FuseTriggerRadius = 200.f;         
	// 이 시간 넘게 못 닿으면 제자리 자폭
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Chase", meta = (ClampMin = "0.0"))
	float MaxChaseTime = 6.f;                 

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Chase", meta = (ClampMin = "0.01"))
	float ChasePollInterval = 0.1f;
	// 0 또는 1이면 미적용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Chase", meta = (ClampMin = "0.0"))
	float ChaseSpeedMultiplier = 1.3f;        
	// 없으면 즉시 폭발
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Fuse")
	TSoftObjectPtr<UAnimMontage> FuseMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Explosion", meta = (ClampMin = "0.0"))
	float ExplosionRadius = 350.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Explosion")
	FName ExplosionBoneName = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Explosion")
	FVector ExplosionOffset = FVector::ZeroVector;
	// 자기 사망용 치사 GE (Health를 0으로 만드는 Override GE)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Death")
	TSubclassOf<UGameplayEffect> SelfKillEffectClass;
	// 기존 GE를 그대로 지정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Explosion")
	TSubclassOf<UGameplayEffect> ExplosionDamageEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Explosion", meta = (ClampMin = "0.0"))
	float BaseDamageMul = 1.0f;

	// 잔여 HP 비율(0~1)에 따른 계수
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Explosion")
	float MulAtZeroHp = 0.5f;   // HP 0%
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SelfDestruct|Explosion")
	float MulAtFullHp = 1.5f;   // HP 100%
private:
	void BeginChase();
	void TickChase();
	void BeginFuse();

	UFUNCTION()
	void OnFuseFinished();

	void Detonate();
	void KillSelf();

	void ApplyChaseSpeed();

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> ChaseTarget;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	FTimerHandle ChasePollTimer;
	FTimerHandle HitboxOffTimer;

	float ChaseElapsed = 0.f;
	float OriginalMaxWalkSpeed = 0.f;
	bool bFuseStarted = false;
	bool bDetonated = false;
};
