#pragma once

#include "CoreMinimal.h"
#include "Combat/RetrieveCombatTypes.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "StaffProjectile.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 * 플레이어 스태프(원거리) 투사체.
 */
UCLASS(Blueprintable, BlueprintType)
class RETRIEVE_API AStaffProjectile : public AActor
{
	GENERATED_BODY()

public:
	AStaffProjectile();
	
	UFUNCTION(BlueprintCallable, Category = "StaffProjectile")
	void Launch(const FVector& Direction, float Speed = 1800.f);
	
	void ConfigureAttack(
		UAbilitySystemComponent* InSourceASC,
		AActor* InInstigatorActor,
		float InDamageMultiplier,
		ERetrieveHitReactType InHitReactType,
		const FGameplayTag& InAttackTypeTag,
		const FGameplayTag& InElementTag,
		TSubclassOf<UGameplayEffect> InElementStatusEffect,
		const FGameplayTag& InChargeBonusEventTag);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnProjectileOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnProjectileStopped(const FHitResult& ImpactResult);

private:
	bool IsIgnoredActor(const AActor* OtherActor) const;
	UAbilitySystemComponent* ResolveSourceASC() const;
	void ApplyHitToTarget(AActor* TargetActor, UAbilitySystemComponent* TargetASC, const FHitResult& SweepResult);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StaffProjectile", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StaffProjectile", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StaffProjectile", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StaffProjectile|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	// TODO(하민): 넉백(임시). 넉백 GA/GE 단일화 시 교체.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StaffProjectile|Knockback", meta = (ClampMin = "0.0"))
	float KnockbackStrength = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StaffProjectile|Knockback", meta = (ClampMin = "0.0"))
	float KnockbackUpwardStrength = 0.f;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<UAbilitySystemComponent> SourceASC;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> InstigatorActor;

	float DamageMultiplier = 1.f;
	ERetrieveHitReactType HitReactType = ERetrieveHitReactType::Flinch;
	FGameplayTag AttackTypeTag;
	FGameplayTag ElementTag;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> ElementStatusEffect;

	FGameplayTag ChargeBonusEventTag;
	
	bool bConsumed = false;
};
