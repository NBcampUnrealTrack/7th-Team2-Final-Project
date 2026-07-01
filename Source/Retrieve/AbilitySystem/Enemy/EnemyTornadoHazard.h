#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyTornadoHazard.generated.h"

class UCapsuleComponent;
class UGameplayEffect;
class UNiagaraComponent;
class USceneComponent;

UCLASS(Blueprintable, BlueprintType)
class RETRIEVE_API AEnemyTornadoHazard : public AActor
{
	GENERATED_BODY()

public:
	AEnemyTornadoHazard();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void UpdateTrackingTarget(float DeltaSeconds);
	void MoveToTrackingTarget(float DeltaSeconds);
	void UpdateTrackedTargets();
	void CollectCurrentTargets(TSet<TWeakObjectPtr<AActor>>& OutTargets) const;
	void ApplyPullToTargets();
	bool ApplyDamageEffect(AActor* TargetActor);
	bool ApplyStatusEffect(AActor* TargetActor);
	bool ShouldAffectTarget(const AActor* TargetActor) const;
	AActor* FindNearestTarget() const;
	void DrawDebugCollision(float DeltaSeconds);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="EnemyTornadoHazard")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="EnemyTornadoHazard|Collision")
	TObjectPtr<UCapsuleComponent> EffectArea;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="EnemyTornadoHazard|Visual")
	TObjectPtr<UNiagaraComponent> TornadoVFXComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyTornadoHazard|Movement")
	bool bTrackTarget = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyTornadoHazard|Movement", meta=(EditCondition="bTrackTarget", ClampMin="0.0"))
	float TrackSpeed = 500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyTornadoHazard|Movement", meta=(EditCondition="bTrackTarget", ClampMin="0.0"))
	float TargetSearchRadius = 3000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyTornadoHazard|Movement", meta=(EditCondition="bTrackTarget", ClampMin="0.01"))
	float TargetSearchInterval = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyTornadoHazard|Pull", meta=(ClampMin="0.0"))
	float PullSpeed = 900.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyTornadoHazard|Pull", meta=(ClampMin="0.0"))
	float UpwardVelocity = 500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyTornadoHazard|Timing", meta=(ClampMin="0.0"))
	float LifeTime = 5.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyTornadoHazard|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyTornadoHazard|Damage")
	TSubclassOf<UGameplayEffect> StatusEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyTornadoHazard|Damage")
	bool bApplyStatusEffectOnDamageTick = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyTornadoHazard|Damage")
	bool bApplyStatusEffectOnExit = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyTornadoHazard|Damage", meta=(ClampMin="0.0"))
	float DamageMultiplier = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyTornadoHazard|Damage", meta=(ClampMin="0.0"))
	float StatusEffectDamageMultiplier = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyTornadoHazard|Damage", meta=(ClampMin="0.01"))
	float DamageInterval = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyTornadoHazard|Target")
	bool bAffectHostileOnly = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyTornadoHazard|Debug")
	bool bDrawDebugCollision = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyTornadoHazard|Debug", meta=(EditCondition="bDrawDebugCollision", ClampMin="0.01"))
	float DebugDrawInterval = 0.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyTornadoHazard|Debug", meta=(EditCondition="bDrawDebugCollision", ClampMin="0.0"))
	float DebugDrawDuration = 3.f;

private:
	float ElapsedTime = 0.f;
	float TargetSearchElapsedTime = 0.f;
	float DebugDrawElapsedTime = 0.f;
	TWeakObjectPtr<AActor> TrackingTarget;
	TMap<TWeakObjectPtr<AActor>, float> ActiveTargets;
};
