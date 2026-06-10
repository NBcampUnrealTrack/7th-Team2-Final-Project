#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyProjectile.generated.h"

class UGameplayEffect;
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

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnProjectileOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="EnemyProjectile", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="EnemyProjectile", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="EnemyProjectile", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyProjectile|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyProjectile|Knockback", meta=(ClampMin="0.0"))
	float KnockbackStrength = 800.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="EnemyProjectile|Knockback", meta=(ClampMin="0.0"))
	float KnockbackUpwardStrength = 400.f;

private:
	bool IsIgnoredActor(const AActor* OtherActor) const;
};
