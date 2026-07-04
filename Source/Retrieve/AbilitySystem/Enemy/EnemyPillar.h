#pragma once

#include "CoreMinimal.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameFramework/Actor.h"
#include "EnemyPillar.generated.h"

class UCapsuleComponent;
class UDecalComponent;
class UGameplayEffect;
class UNiagaraComponent;
class UNiagaraSystem;
class UPrimitiveComponent;
class USceneComponent;
class USoundBase;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EEnemyPillarPhase : uint8
{
	Warning,
	Rising,
	Active,
	Finished
};

UCLASS(Blueprintable, BlueprintType)
class RETRIEVE_API AEnemyPillar : public AActor
{
	GENERATED_BODY()

public:
	AEnemyPillar();

	UFUNCTION(BlueprintCallable, Category="EnemyPillar")
	void SetLaunchKnockbackConfig(
		const FMonsterLaunchKnockbackConfig& InLaunchKnockbackConfig);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void EnterPhase(EEnemyPillarPhase NewPhase);
	void UpdatePhase(float DeltaSeconds);
	void UpdateWarningVisual(float PhaseAlpha);
	void UpdateRisingVisual(float PhaseAlpha);
	void ApplyContactEffects(AActor* TargetActor);
	void ApplyPeriodicDamage();
	void ApplyPostExposureEffect(AActor* TargetActor);
	void ApplyPostExposureEffectToTrackedActors();
	void ApplyLaunchKnockback(AActor* TargetActor) const;
	bool ApplyEffect(
		AActor* TargetActor,
		TSubclassOf<UGameplayEffect> EffectClass,
		float DamageMultiplier = -1.f);
	bool ShouldApplyEffectTo(const AActor* OtherActor) const;
	bool IsEffectPhase(EEnemyPillarPhase Phase) const;
	float GetPhaseDuration(EEnemyPillarPhase Phase) const;

	UFUNCTION()
	void OnEffectAreaBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnRetentionAreaEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="EnemyPillar")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="EnemyPillar")
	TObjectPtr<USceneComponent> PillarRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="EnemyPillar|Visual")
	TObjectPtr<UDecalComponent> WarningDecal;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="EnemyPillar|Visual")
	TObjectPtr<UStaticMeshComponent> PillarMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="EnemyPillar|Collision")
	TObjectPtr<UCapsuleComponent> EffectArea;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="EnemyPillar|Collision")
	TObjectPtr<UCapsuleComponent> RetentionArea;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="EnemyPillar|Visual")
	TObjectPtr<UNiagaraComponent> RiseVFXComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyPillar|Visual")
	TObjectPtr<UNiagaraSystem> EndVFXSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyPillar|Audio")
	TObjectPtr<USoundBase> IceBreakSFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyPillar|Timing", meta=(ClampMin="0.0"))
	float WarningDuration = 1.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyPillar|Timing", meta=(ClampMin="0.0"))
	float RiseDuration = 1.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyPillar|Timing", meta=(ClampMin="0.0"))
	float ActiveDuration = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyPillar|Contact")
	TSubclassOf<UGameplayEffect> ContactDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyPillar|Contact", meta=(ClampMin="0.0"))
	float ContactDamageMultiplier = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyPillar|Post Exposure")
	TSubclassOf<UGameplayEffect> PostExposureEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyPillar|Periodic Damage")
	TSubclassOf<UGameplayEffect> PeriodicDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyPillar|Periodic Damage", meta=(ClampMin="0.0"))
	float PeriodicDamageMultiplier = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyPillar|Periodic Damage", meta=(ClampMin="0.01"))
	float PeriodicDamageInterval = 1.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="EnemyPillar|Knockback")
	FMonsterLaunchKnockbackConfig LaunchKnockbackConfig;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="EnemyPillar|State")
	EEnemyPillarPhase CurrentPhase = EEnemyPillarPhase::Warning;

private:
	FVector EffectAreaFinalRelativeLocation = FVector::ZeroVector;
	FVector InitialWarningDecalSize = FVector::ZeroVector;
	float PhaseElapsedTime = 0.f;
	FTimerHandle PeriodicDamageTimerHandle;
	TSet<TWeakObjectPtr<AActor>> TrackedContactActors;
};
