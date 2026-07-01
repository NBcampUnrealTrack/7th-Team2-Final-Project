#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyWaveHazard.generated.h"

class UBoxComponent;
class UGameplayEffect;
class UNiagaraComponent;
class USceneComponent;

UCLASS(Blueprintable, BlueprintType)
class RETRIEVE_API AEnemyWaveHazard : public AActor
{
	GENERATED_BODY()

public:
	AEnemyWaveHazard();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void UpdateCollisionMovement(float DeltaSeconds);
	void DrawDebugCollision(float DeltaSeconds);
	void UpdateTrackedTargets();
	void CollectCurrentTargets(TSet<TWeakObjectPtr<AActor>>& OutTargets) const;
	void UpdateCarriedTargets();
	bool ApplyDamageEffect(AActor* TargetActor);
	bool ApplyStatusEffect(AActor* TargetActor);
	bool ShouldAffectTarget(const AActor* TargetActor) const;
	float GetMovedDistance() const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="EnemyWaveHazard")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="EnemyWaveHazard|Collision")
	TObjectPtr<UBoxComponent> EffectArea;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="EnemyWaveHazard|Visual")
	TObjectPtr<UNiagaraComponent> WaveVFXComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyWaveHazard|Movement", meta=(ClampMin="0.0"))
	float MoveSpeed = 900.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyWaveHazard|Movement", meta=(ClampMin="0.0"))
	float MoveAcceleration = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyWaveHazard|Movement", meta=(ClampMin="0.0"))
	float MaxMoveSpeed = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyWaveHazard|Movement", meta=(ClampMin="0.0"))
	float MaxTravelDistance = 1200.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyWaveHazard|Movement")
	bool bUseCarry = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyWaveHazard|Movement", meta=(EditCondition="bUseCarry", ClampMin="0.0"))
	float CarrySpeed = 700.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyWaveHazard|Timing", meta=(ClampMin="0.0"))
	float LifeTime = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyWaveHazard|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyWaveHazard|Damage")
	TSubclassOf<UGameplayEffect> StatusEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyWaveHazard|Damage")
	bool bApplyStatusEffectOnDamageTick = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyWaveHazard|Damage")
	bool bApplyStatusEffectOnExit = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyWaveHazard|Damage", meta=(ClampMin="0.0"))
	float StatusEffectDamageMultiplier = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyWaveHazard|Damage", meta=(ClampMin="0.0"))
	float DamageMultiplier = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyWaveHazard|Damage", meta=(ClampMin="0.01"))
	float DamageInterval = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyWaveHazard|Target")
	bool bAffectHostileOnly = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyWaveHazard|Debug")
	bool bDrawDebugCollision = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyWaveHazard|Debug", meta=(EditCondition="bDrawDebugCollision", ClampMin="0.01"))
	float DebugDrawInterval = 0.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyWaveHazard|Debug", meta=(EditCondition="bDrawDebugCollision", ClampMin="0.0"))
	float DebugDrawDuration = 3.f;

private:
	FVector InitialEffectAreaRelativeLocation = FVector::ZeroVector;
	float ElapsedTime = 0.f;
	float CurrentMoveSpeed = 0.f;
	float DebugDrawElapsedTime = 0.f;
	TMap<TWeakObjectPtr<AActor>, float> ActiveTargets;
};
