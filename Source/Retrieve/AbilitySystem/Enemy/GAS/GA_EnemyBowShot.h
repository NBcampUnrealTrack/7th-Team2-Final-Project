#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Enemy/GAS/GA_ShootProjectiles.h"
#include "Character/Cosmetics/RetrieveBowMontageSet.h"
#include "GA_EnemyBowShot.generated.h"

class UAbilityTask_PlayMontageAndWait;
class URetrieveBowMeshAnimInstance;
class USkeletalMeshComponent;

UENUM()
enum class EEnemyBowShotStage : uint8
{
	None,
	DrawIntro,
	DrawHold,
	DrawShake,
	Fire,
};

/**
 * 고블린 궁수용 사격 Ability.
 * 캐릭터와 활 메시의 phase 몽타주를 함께 재생하고, 발사 시점에는 기존 적 투사체 패턴을 사용한다.
 */
UCLASS(Blueprintable, BlueprintType)
class RETRIEVE_API UGA_EnemyBowShot : public UGA_ShootProjectiles
{
	GENERATED_BODY()

public:
	UGA_EnemyBowShot(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	virtual const UAnimMontage* ResolveMontage(const FGameplayEventData* TriggerEventData) const override;
	virtual void OnSpecialAttackActivated() override;
	virtual void OnSpecialAttackEnded() override;
	virtual void OnBeforeProjectileSpawn() override;
	virtual void OnProjectileSpawned(AEnemyProjectile* Projectile, AActor* AvatarActor) override;
	virtual bool ShouldScheduleProjectilesOnActivate() const override { return false; }
	virtual USkeletalMeshComponent* ResolveProjectileSpawnMesh(AActor* AvatarActor) const override;
	virtual FVector ResolveAimedProjectileDirection(const FVector& SpawnLocation, AActor* TargetActor) const override;
	virtual void OnMontageCompleted() override;
	virtual void OnMontageInterrupted() override;

	/** 고블린 캐릭터 메시에 재생할 Standing phase 몽타주 세트. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy Bow|Animation")
	FRetrieveBowMontageSet CharacterShotMontages;

	/** Drawn 루프를 유지할 시간. 공격 속도 배율의 영향을 받는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy Bow|Timing", meta=(ClampMin="0.0"))
	float DrawHoldDuration = 0.45f;

	/** DrawnShake 루프를 유지할 시간. 0이면 해당 단계를 건너뛴다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy Bow|Timing", meta=(ClampMin="0.0"))
	float DrawShakeDuration = 0.35f;

	/** true면 발사 후 다시 장전하는 FireReload, false면 FireIdle을 사용한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy Bow|Animation")
	bool bUseFireReloadMontage = true;

	/** Bounds 중심에서 추가로 보정할 조준 위치. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy Bow|Trajectory")
	FVector TargetAimOffset = FVector::ZeroVector;

	/** 중력 사용 패턴에서 저각 탄도해를 계산한다. 해가 없으면 직선 조준으로 폴백한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemy Bow|Trajectory")
	bool bUseBallisticArc = true;

private:
	void CacheBowMesh();
	void FaceTarget() const;
	void StartDrawHold();
	void StartDrawShake();
	void StartFire();
	void ScheduleNextPhase(float BaseDuration, void (UGA_EnemyBowShot::*Callback)());
	bool PlayCharacterPhase(EBowShotPhase Phase, bool bWaitForCompletion);
	void PlayBowMeshPhase(EBowShotPhase Phase) const;
	void StopPhaseMontageTask();

	UFUNCTION()
	void HandleFireMontageFinished();

	UFUNCTION()
	void HandleFireMontageInterrupted();

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> PhaseMontageTask;

	TWeakObjectPtr<USkeletalMeshComponent> CachedBowMeshComponent;
	TWeakObjectPtr<URetrieveBowMeshAnimInstance> CachedBowMeshAnimInstance;
	FTimerHandle PhaseTimerHandle;
	EEnemyBowShotStage ShotStage = EEnemyBowShotStage::None;
};
