#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Enemy/GAS/GA_ShootProjectiles.h"
#include "GA_EpicShootProjectiles.generated.h"

class ACharacter;
class ARetrieveEnemyCharacter;
class UAnimSequenceBase;

UCLASS(Blueprintable, BlueprintType)
class RETRIEVE_API UGA_EpicShootProjectiles : public UGA_ShootProjectiles
{
	GENERATED_BODY()

public:
	UGA_EpicShootProjectiles(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual const UAnimMontage* ResolveMontage(const FGameplayEventData* TriggerEventData) const override;
	virtual UAnimMontage* ResolveFallbackSequenceMontage() const override;
	virtual void OnSpecialAttackActivated() override;
	virtual void OnSpecialAttackEnded() override;
	virtual void OnBeforeProjectileSpawn() override;
	virtual void OnProjectileSpawned(AEnemyProjectile* Projectile, AActor* AvatarActor) override;
	virtual float AdjustProjectileFireDelay(float FireDelay, int32 ProjectileIndex) const override;
	virtual bool UsesProjectileCompletionGuard() const override { return true; }
	virtual void FinishAbility() override;
	virtual void OnMontageCompleted() override;
	virtual void OnMontageInterrupted() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Epic|ShootProjectiles")
	TSoftObjectPtr<UAnimSequenceBase> FallbackMontageSequence;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Epic|Aerial")
	bool bUseAerialModeOnActivate = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Epic|Aerial")
	TArray<FName> AerialMonsterDataRows;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Epic|Aerial", meta=(ClampMin="0.0"))
	float AerialLiftHeight = 350.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Epic|Aerial", meta=(ClampMin="0.0"))
	float AerialLiftDuration = 0.85f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Epic|Aerial", meta=(ClampMin="0.0"))
	float MinimumAerialModeDuration = 2.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Epic|Aerial|Animation")
	TSoftObjectPtr<UAnimSequenceBase> AerialTakeOffAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Epic|Aerial|Animation")
	TSoftObjectPtr<UAnimSequenceBase> AerialHoverAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Epic|Aerial|Animation")
	TSoftObjectPtr<UAnimSequenceBase> AerialLandingAnimation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Epic|Aerial|Animation")
	FName AerialAnimationSlot = FName(TEXT("DefaultSlot"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Epic|Aerial|Animation", meta=(ClampMin="0.1"))
	float AerialAnimationPlayRate = 1.f;

private:
	void FaceCachedTarget() const;
	bool ShouldApplyAerialMode(const ARetrieveEnemyCharacter* Enemy) const;
	void StartAerialLift();
	void UpdateAerialLift();
	UAnimMontage* CreateAerialAnimationMontage(const TSoftObjectPtr<UAnimSequenceBase>& Animation, int32 LoopCount) const;
	void PlayAerialHoverAnimation();
	void PlayAerialLandingAnimation();
	void ForceAerialLandingIfStillFlying();
	float GetAerialModeElapsedTime() const;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> CachedAvatarCharacter;

	FTimerHandle AerialFinishTimerHandle;
	FTimerHandle AerialLiftTimerHandle;
	FVector AerialLiftStartLocation = FVector::ZeroVector;
	FVector AerialLiftTargetLocation = FVector::ZeroVector;
	float AerialModeStartTime = 0.f;
	float AerialLiftStartTime = 0.f;
	bool bFinishingAfterAerialHold = false;
	bool bAerialModeApplied = false;
	bool bAerialLiftInProgress = false;
	bool bAerialHoverAnimationPlaying = false;
};
