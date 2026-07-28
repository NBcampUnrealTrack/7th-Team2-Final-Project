
#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GA_BossQueen_AerialDive.generated.h"

class ARetrieveEnemyCharacter;
class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;
class UCharacterMovementComponent;
class UEnemyCombatComponent;

UCLASS(Blueprintable, BlueprintType)
class RETRIEVE_API UGA_BossQueen_AerialDive : public URetrieveGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_BossQueen_AerialDive(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void ActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

protected:
	bool ResolveAerialDiveConfig(); 

	FMonsterAerialDiveConfig ActiveDiveConfig;

	UPROPERTY(EditDefaultsOnly, Category="Aerial Dive|Animation")
	FName TakeoffSection = TEXT("Takeoff");

	UPROPERTY(EditDefaultsOnly, Category="Aerial Dive|Animation")
	FName AimSection = TEXT("Aim");

	UPROPERTY(EditDefaultsOnly, Category="Aerial Dive|Animation")
	FName DiveSection = TEXT("Dive");

private:
	enum class EDiveStage : uint8
	{
		None,
		Rising,
		Aiming,
		Diving,
	};

	void TickMotion();
	void BeginAiming();
	void BeginDiving();
	void FinishDive();
	void FinishAbility(bool bWasCancelled);
	
	void FaceLocation(const FVector& Location);
	void SetMontageSection(FName SectionName, bool bLoop);
	bool MoveAvatarSwept(const FVector& Delta, float& OutMovedDistance);

	UFUNCTION()
	void OnMontageFinished();

	UFUNCTION()
	void OnMontageInterrupted();

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	TWeakObjectPtr<ARetrieveEnemyCharacter> CachedEnemy;
	TWeakObjectPtr<UEnemyCombatComponent> CachedCombat;
	TWeakObjectPtr<UCharacterMovementComponent> CachedMovement;
	TWeakObjectPtr<const AActor> CachedTarget;

	FTimerHandle MotionTimerHandle;

	EDiveStage Stage = EDiveStage::None;

	FVector TakeoffTarget = FVector::ZeroVector;
	FVector LockedTargetLocation = FVector::ZeroVector;
	FVector DiveDirection = FVector::ZeroVector;

	float StageElapsed = 0.f;
	float TotalElapsed = 0.f;
	float DiveDistance = 0.f;
	float DiveTravelled = 0.f;
	float LastUpdateTime = 0.f;
	
};
