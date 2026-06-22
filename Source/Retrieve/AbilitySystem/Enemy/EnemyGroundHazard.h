#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyGroundHazard.generated.h"

class UDecalComponent;
class UGameplayEffect;
class UMaterialInstanceDynamic;
class UPrimitiveComponent;
class USceneComponent;
class USphereComponent;
class ACharacter;

UENUM(BlueprintType)
enum class EEnemyGroundHazardPhase : uint8
{
	Warning,
	Expanding,
	Active,
	Fading,
	Finished
};

UCLASS(Blueprintable, BlueprintType)
class RETRIEVE_API AEnemyGroundHazard : public AActor
{
	GENERATED_BODY()

public:
	AEnemyGroundHazard();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void EnterPhase(EEnemyGroundHazardPhase NewPhase);
	void UpdatePhase(float DeltaSeconds);
	void UpdateVisuals(float PhaseAlpha);
	void SetDecalRadius(UDecalComponent* Decal, float Radius) const;
	void ApplyPeriodicEffects();
	void ApplyPostExposureEffect(AActor* TargetActor);
	void ApplyPostExposureEffectToTrackedActors();
	bool ApplyEffect(
		AActor* TargetActor,
		TSubclassOf<UGameplayEffect> EffectClass,
		float DamageMultiplier = -1.f,
		float DurationOverride = -1.f);
	bool ShouldApplyDamageTo(const AActor* OtherActor) const;
	bool IsInsideEffectiveArea(const AActor* TargetActor) const;
	bool IsDamagePhase(EEnemyGroundHazardPhase Phase) const;
	float GetPhaseDuration(EEnemyGroundHazardPhase Phase) const;
	float GetCollisionRadius() const;

	void ApplyContinuousPushTo(AActor* TargetActor);
	void UpdateContinuousPushTargets();
	void RemoveContinuousPushFrom(AActor* TargetActor);
	void RemoveAllContinuousPushSources();

	UFUNCTION()
	void OnDamageAreaBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnDamageAreaEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="EnemyGroundHazard")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="EnemyGroundHazard|Collision")
	TObjectPtr<USphereComponent> DamageArea;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="EnemyGroundHazard|Visual")
	TObjectPtr<UDecalComponent> CenterDecal;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="EnemyGroundHazard|Visual")
	TObjectPtr<UDecalComponent> ExpandingDecal;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyGroundHazard|Timing", meta=(ClampMin="0.0"))
	float WarningDuration = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyGroundHazard|Timing", meta=(ClampMin="0.0"))
	float ExpandDuration = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyGroundHazard|Timing", meta=(ClampMin="0.0"))
	float ActiveDuration = 4.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyGroundHazard|Timing", meta=(ClampMin="0.0"))
	float FadeDuration = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyGroundHazard|Size", meta=(ClampMin="0.0"))
	float CenterRadius = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyGroundHazard|Size", meta=(ClampMin="0.0"))
	float InitialRadius = 150.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyGroundHazard|Size", meta=(ClampMin="0.0"))
	float MaxRadius = 500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyGroundHazard|Size", meta=(ClampMin="0.0"))
	float MaxVerticalDifference = 75.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyGroundHazard|Collision",
		meta=(ClampMin="0.0", ClampMax="1.0"))
	float CollisionRadiusScale = 0.9f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyGroundHazard|Continuous Push")
	bool bUseContinuousPush = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyGroundHazard|Continuous Push",
		meta=(ClampMin="0.0", EditCondition="bUseContinuousPush", EditConditionHides))
	float ContinuousPushSpeed = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyGroundHazard|Damage")
	TSubclassOf<UGameplayEffect> DirectDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyGroundHazard|Damage", meta=(ClampMin="0.0"))
	float DirectDamageMultiplier = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyGroundHazard|Damage", meta=(ClampMin="0.01"))
	float DirectDamageInterval = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyGroundHazard|Status Effect")
	TSubclassOf<UGameplayEffect> PeriodicStatusEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyGroundHazard|Post Exposure")
	TSubclassOf<UGameplayEffect> PostExposureEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyGroundHazard|Post Exposure", meta=(ClampMin="0.0"))
	float PostExposureDamageMultiplier = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyGroundHazard|Post Exposure", meta=(ClampMin="0.0"))
	float PostExposureDuration = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyGroundHazard|Visual")
	FName OpacityParameterName = TEXT("Opacity");

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="EnemyGroundHazard|State")
	EEnemyGroundHazardPhase CurrentPhase = EEnemyGroundHazardPhase::Warning;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="EnemyGroundHazard|State")
	float CurrentRadius = 0.f;

private:
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> CenterMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ExpandingMaterial;

	float PhaseElapsedTime = 0.f;
	FTimerHandle PeriodicEffectTimerHandle;
	TSet<TWeakObjectPtr<AActor>> ActorsInsideDamageArea;
	TMap<TWeakObjectPtr<ACharacter>, uint16> ActivePushRootMotionSourceIds;
};
