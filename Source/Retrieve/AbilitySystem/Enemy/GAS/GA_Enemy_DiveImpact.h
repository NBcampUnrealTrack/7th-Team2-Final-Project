#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Enemy/GAS/GA_EnemyPatternAbilityBase.h"
#include "GA_Enemy_DiveImpact.generated.h"

class ARetrieveEnemyCharacter;
class UAbilityTask_PlayMontageAndWait;
class UAnimMontage;
class UDecalComponent;
class UMaterialInterface;
class UNiagaraSystem;

UENUM()
enum class EEnemyDiveImpactPhase : uint8
{
	None,
	TakeOff,
	AirFollow,
	Dive,
};

UCLASS(Blueprintable, BlueprintType)
class RETRIEVE_API UGA_Enemy_DiveImpact : public UGA_EnemyPatternAbilityBase
{
	GENERATED_BODY()

public:
	UGA_Enemy_DiveImpact(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

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
	virtual void OnMontageCompleted() override;

	virtual void OnMontageInterrupted() override;

private:
	UAnimMontage* ResolveAttackMontage(const FGameplayEventData* TriggerEventData) const;
	const AActor* ResolveTargetActor(const FGameplayEventData* TriggerEventData) const;

	void BeginTakeOff();
	void BeginAirFollow();
	void BeginDive();
	void TickDiveMotion();
	void FinishDive();
	void FinishAbility(bool bWasCancelled);

	void SpawnWarningDecal();
	bool ResolveGroundLocation(const FVector& SourceLocation, FVector& OutGroundLocation) const;
	void FaceLocation(const FVector& Location) const;
	void ApplyImpact();
	void FadeWarningDecal();
	void SetDiveCapsulePawnOverlap();
	void RestoreDiveCapsulePawnResponse();
	
protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive Impact|Impact VFX")
	TObjectPtr<UNiagaraSystem> ImpactVFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive Impact|Impact VFX")
	FVector ImpactVFXScale = FVector(1.f);
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive Impact|Animation", meta=(ClampMin="0.01"))
	float MontagePlayRate = 1.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive Impact|Animation")
	FName ImpactSectionName = TEXT("Impact");
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive Impact|Movement", meta=(ClampMin="0.0"))
	float TakeOffHeight = 1200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive Impact|Movement", meta=(ClampMin="0.01"))
	float TakeOffDuration = 0.6f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive Impact|Movement", meta=(ClampMin="0.0"))
	float AirFollowDuration = 1.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive Impact|Movement", meta=(ClampMin="0.01"))
	float AirFollowInterpSpeed = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive Impact|Movement", meta=(ClampMin="0.01"))
	float DiveDuration = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive Impact|Movement", meta=(ClampMin="0.001"))
	float MotionTickInterval = 0.016f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive Impact|Ground Trace", meta=(ClampMin="0.0"))
	float GroundTraceUpDistance = 500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive Impact|Ground Trace", meta=(ClampMin="0.0"))
	float GroundTraceDownDistance = 3000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive Impact|Ground Trace")
	float GroundOffset = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive Impact|Warning", meta=(ClampMin="0.0"))
	float WarningLeadTime = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive Impact|Warning", meta=(ClampMin="0.0"))
	float WarningDecalRadius = 450.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive Impact|Warning", meta=(ClampMin="0.0"))
	float WarningDecalDepth = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive Impact|Warning", meta=(ClampMin="0.0"))
	float WarningDecalFadeOutDuration = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive Impact|Warning")
	TObjectPtr<UMaterialInterface> WarningDecalMaterial;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive Impact|Movement", meta=(ClampMin="0.0"))
	float AirFollowLocationTolerance = 30.f;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Dive Impact|Collision", meta=(ClampMin="0.0"))
	float RestoreTime = 0.15f;
private:
	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> ActiveMontage;

	TWeakObjectPtr<ARetrieveEnemyCharacter> CachedEnemy;
	TWeakObjectPtr<const AActor> CachedTarget;
	TWeakObjectPtr<UDecalComponent> WarningDecal;

	FTimerHandle MotionTimerHandle;
	
	FTimerHandle CollisionRestoreTimerHandle;
	
	EEnemyDiveImpactPhase Phase = EEnemyDiveImpactPhase::None;
	FVector PhaseStartLocation = FVector::ZeroVector;
	FVector HoverLocation = FVector::ZeroVector;
	FVector DiveTargetLocation = FVector::ZeroVector;

	float PhaseElapsed = 0.f;
	float LastUpdateTime = 0.f;
	bool bWarningSpawned = false;
	
	ECollisionResponse CachedCapsulePawnResponse = ECR_Block;
	bool bDidOverrideCapsulePawnResponse = false;
};
