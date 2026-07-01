#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystem/Enemy/GAS/GA_ShootProjectiles.h"
#include "GA_Enemy_WindRetreatSlash.generated.h"

class UAbilityTask_WaitGameplayEvent;

UCLASS(Blueprintable, BlueprintType)
class RETRIEVE_API UGA_Enemy_WindRetreatSlash : public UGA_ShootProjectiles
{
	GENERATED_BODY()

public:
	UGA_Enemy_WindRetreatSlash(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void EndAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

protected:
	virtual void OnSpecialAttackActivated() override;
	virtual void OnSpecialAttackEnded() override;
	virtual float AdjustProjectileFireDelay(float FireDelay, int32 ProjectileIndex) const override;
	virtual bool ShouldScheduleProjectilesOnActivate() const override { return false; }
	virtual bool UsesProjectileCompletionGuard() const override { return true; }

private:
	UFUNCTION()
	void HandleRetreatStartEvent(FGameplayEventData Payload);

	UFUNCTION()
	void HandleRetreatFireEvent(FGameplayEventData Payload);

	void PrepareRetreatMovement();
	void StartRetreatMovement();
	void TickRetreat();
	void FinishRetreatMovement();
	void ApplyJump();
	void FaceTarget() const;
	FVector GetRetreatDirection() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wind Retreat Slash|Movement", meta=(AllowPrivateAccess="true", ClampMin="0.0"))
	float TotalRetreatDistance = 700.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wind Retreat Slash|Movement", meta=(AllowPrivateAccess="true", ClampMin="0.0"))
	float JumpVerticalVelocity = 700.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wind Retreat Slash|Movement", meta=(AllowPrivateAccess="true", ClampMin="0.01"))
	float RetreatMoveDuration = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Wind Retreat Slash|Movement", meta=(AllowPrivateAccess="true", ClampMin="0.001"))
	float RetreatTickInterval = 0.016f;

	FTimerHandle RetreatTimerHandle;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> RetreatStartEventTask;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> RetreatFireEventTask;

	FVector RetreatStartLocation = FVector::ZeroVector;
	FVector RetreatFinalLocation = FVector::ZeroVector;
	float RetreatElapsed = 0.f;
	bool bRetreatMovementPrepared = false;
	bool bRetreatMovementStarted = false;
	bool bProjectilesScheduled = false;
};
