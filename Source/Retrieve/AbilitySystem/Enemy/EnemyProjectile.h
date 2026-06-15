#pragma once

#include "CoreMinimal.h"
#include "Combat/RetrieveCombatTypes.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameFramework/Actor.h"
#include "EnemyProjectile.generated.h"

class UGameplayEffect;
class UNiagaraComponent;
class UNiagaraSystem;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable, BlueprintType)
class RETRIEVE_API AEnemyProjectile : public AActor
{
	GENERATED_BODY()

public:
	AEnemyProjectile();

	UFUNCTION(BlueprintCallable, Category="EnemyProjectile")
	void Launch(const FVector& Direction, float Speed = 1200.f);
	
	UFUNCTION(BlueprintCallable, Category="EnemyProjectile")
	void ConfigureHoming(AActor* TargetActor, float StartDelay, float Duration, float Strength);
	
	UFUNCTION(BlueprintCallable, Category="EnemyProjectile")
	void SetProjectileLifetime(float Lifetime);
	
	UFUNCTION(BlueprintCallable, Category="EnemyProjectile")
	void SetGravityScale(float GravityScale);

	UFUNCTION(BlueprintCallable, Category="EnemyProjectile")
	void SetHitReactType(ERetrieveHitReactType InHitReactType);

	UFUNCTION(BlueprintCallable, Category="EnemyProjectile")
	void SetLaunchKnockbackConfig(const FMonsterLaunchKnockbackConfig& InLaunchKnockbackConfig);
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UFUNCTION()
	void OnProjectileOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnProjectileStopped(const FHitResult& ImpactResult);
	
private:
	bool IsIgnoredActor(const AActor* OtherActor) const;
	void PlayImpactVFX(const FVector& Location, const FRotator& Rotation);
	void StartHoming(float Strength);
	void StopHoming();
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="EnemyProjectile", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="EnemyProjectile", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="EnemyProjectile|VFX", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UNiagaraComponent> FlightVFXComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="EnemyProjectile", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyProjectile|VFX")
	TObjectPtr<UNiagaraSystem> ImpactVFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyProjectile|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyProjectile|Knockback")
	FMonsterLaunchKnockbackConfig LaunchKnockbackConfig;

	UPROPERTY(Transient)
	ERetrieveHitReactType HitReactType = ERetrieveHitReactType::Flinch;
	
private:
	UPROPERTY(Transient)
	TObjectPtr<AActor> HomingTargetActor;

	FTimerHandle HomingStartTimerHandle;
	FTimerHandle HomingStopTimerHandle;
};
