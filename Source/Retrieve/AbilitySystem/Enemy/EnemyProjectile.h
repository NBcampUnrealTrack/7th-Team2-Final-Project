#pragma once

#include "CoreMinimal.h"
#include "Combat/RetrieveCombatTypes.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameFramework/Actor.h"
#include "EnemyProjectile.generated.h"

class UAbilitySystemComponent;
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
	void PrepareForDelayedLaunch();

	UFUNCTION(BlueprintCallable, Category="EnemyProjectile")
	void ReleaseDelayedLaunch(
		const FVector& Direction,
		float Speed,
		float Lifetime,
		float GravityScale = 0.f);
	
	UFUNCTION(BlueprintCallable, Category="EnemyProjectile")
	void ConfigureHoming(AActor* TargetActor, float StartDelay, float Duration, float Strength);
	
	UFUNCTION(BlueprintCallable, Category="EnemyProjectile")
	void SetProjectileLifetime(float Lifetime);
	
	UFUNCTION(BlueprintCallable, Category="EnemyProjectile")
	void SetGravityScale(float GravityScale);

	UFUNCTION(BlueprintCallable, Category="EnemyProjectile")
	void SetHitReactType(ERetrieveHitReactType InHitReactType);

	UFUNCTION(BlueprintCallable, Category="EnemyProjectile")
	void SetEffectTag(FGameplayTag InEffectTag);

	UFUNCTION(BlueprintCallable, Category="EnemyProjectile")
	void SetLaunchKnockbackConfig(const FMonsterLaunchKnockbackConfig& InLaunchKnockbackConfig);

	UFUNCTION(BlueprintCallable, Category="EnemyProjectile")
	void SetStatusEffectClass(TSubclassOf<UGameplayEffect> InStatusEffectClass);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnProjectileOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnProjectileStopped(const FHitResult& ImpactResult);

	virtual bool HandleReflectedOverlap(AActor* OtherActor, const FHitResult& SweepResult) { return false; }
	virtual bool TryReflectOnHit(AActor* OtherActor, UAbilitySystemComponent* OtherASC) { return false; }
	virtual bool IsIgnoredActor(const AActor* OtherActor) const;

	// 충돌 정지(지형 등) 시 반경 데미지 적용 여부.
	// 기본값 false → 일반/보스 투사체는 직격 데미지만 적용(원본 동작 유지, 이중 데미지/의도치 않은 AoE 방지).
	// 에픽 AoE 투사체만 override하여 true로 활성화한다.
	virtual bool ShouldApplyImpactRadiusDamage() const { return false; }

	bool IsPlayerTarget(const AActor* OtherActor) const;
	bool ShouldApplyDamageTo(const AActor* OtherActor) const;
	void PlayImpactVFX(const FVector& Location, const FRotator& Rotation);
	bool ApplyDamage(AActor* OtherActor);
	void ApplyDamageInRadius(const FVector& Origin);
	void ApplyLaunchKnockback(AActor* OtherActor, const FVector& Origin);
	void ApplyLaunchKnockbackInRadius(const FVector& Origin);
	void ApplyStatusEffect(AActor* OtherActor);
	void ApplyStatusEffectInRadius(const FVector& Origin);
	void StopHoming();

private:
	void ConfigureNonBlockingComponents();
	void StartHoming(float Strength);

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
	TObjectPtr<UNiagaraSystem> FlightVFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyProjectile|VFX")
	TObjectPtr<UNiagaraSystem> ImpactVFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyProjectile|VFX")
	bool bForceImpactVFXWorldUpRotation = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyProjectile|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyProjectile|Damage", meta=(ClampMin="0.0"))
	float DamageMultiplier = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyProjectile|Knockback")
	FMonsterLaunchKnockbackConfig LaunchKnockbackConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyProjectile|Knockback", meta=(ClampMin="0.0"))
	float LaunchKnockbackRadius = 180.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyProjectile|Status")
	TSubclassOf<UGameplayEffect> StatusEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyProjectile|Status", meta=(ClampMin="0.0"))
	float StatusEffectRadius = 180.f;

	UPROPERTY(Transient)
	ERetrieveHitReactType HitReactType = ERetrieveHitReactType::Flinch;

	UPROPERTY(Transient)
	FGameplayTag EffectTag;
	
private:
	UPROPERTY(Transient)
	TObjectPtr<AActor> HomingTargetActor;

	FTimerHandle HomingStartTimerHandle;
	FTimerHandle HomingStopTimerHandle;
};
