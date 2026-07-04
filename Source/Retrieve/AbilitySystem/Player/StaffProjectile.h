#pragma once

#include "CoreMinimal.h"
#include "Combat/RetrieveCombatTypes.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "StaffProjectile.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;
class UMeshComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;
class UWorld;
class AStaffProjectile;

// 투사체 스폰 파라미터(스태프 강공·활 공용)
struct FRetrieveProjectileSpawnParams
{
	TSubclassOf<AStaffProjectile> ProjectileClass;
	float Speed = 1800.f;
	float DamageMultiplier = 1.f;
	ERetrieveHitReactType HitReactType = ERetrieveHitReactType::Flinch;
	FName SpawnSocketName = NAME_None;
	FVector SpawnOffset = FVector::ZeroVector;
	// true면 SpawnSocketName이 해석된 경우에도 SpawnOffset을 소켓 위치에 (액터 회전 적용) 가산한다.
	// 기본 false = 기존 동작(소켓이 있으면 오프셋 무시). 다중 오프셋(얼음창 좌/상/우) 스폰에서 소켓 기준 배치용.
	bool bAddOffsetToSocketBase = false;
	// >0이면 스폰 후 이 시간만큼 제자리에 떠 있다가 발사(생성→발사 연출). 0이면 즉시 발사.
	float LaunchDelay = 0.f;
	// true면 조준점/타겟/컨트롤 방향을 무시하고 시전자(액터) 정면으로 발사.
	bool bUseActorForward = false;
	FGameplayTag AttackTypeTag;
	FGameplayTag ElementTag;
	TSubclassOf<UGameplayEffect> ElementStatusEffect;
	FGameplayTag ChargeBonusEventTag;

	// ---- 조준 / 낙차 ----
	// 조준 지점(카메라 트레이스 히트). bHasAimPoint가 true면 이 지점으로 직선 조준한다.
	FVector AimPointLocation = FVector::ZeroVector;
	bool bHasAimPoint = false;
	// 0이면 직선 비행(스태프 강공 등). >0이면 중력 낙차(활). 클수록 빨리 떨어진다.
	float GravityScaleOverride = 0.f;
};

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

	// 스폰 후 Delay초 제자리 대기 → 발사(얼음창이 맺혔다가 날아가는 연출). Delay<=0이면 즉시 발사.
	void ArmDelayedLaunch(const FVector& Direction, float Speed, float Delay);

	// 같은 볼리(얼음창 3발 등)끼리 이동 충돌을 무시하도록 등록.
	void IgnoreOtherProjectile(AStaffProjectile* Other);
	
	void ConfigureAttack(
		UAbilitySystemComponent* InSourceASC,
		AActor* InInstigatorActor,
		float InDamageMultiplier,
		ERetrieveHitReactType InHitReactType,
		const FGameplayTag& InAttackTypeTag,
		const FGameplayTag& InElementTag,
		TSubclassOf<UGameplayEffect> InElementStatusEffect,
		const FGameplayTag& InChargeBonusEventTag);

	// 소켓 → 캐릭터메시 → 액터오프셋 위치 + (AimTarget 방향, 없으면 컨트롤 전방)으로 스폰, ConfigureAttack, Launch
	static AStaffProjectile* SpawnConfigured(UWorld* World, AActor* AvatarActor, UAbilitySystemComponent* SourceASC,
		UMeshComponent* WeaponMesh, AActor* AimTarget, const FRetrieveProjectileSpawnParams& Params);

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
	// ArmDelayedLaunch 타이머 콜백: 저장한 방향/속도로 발사.
	void HandleDelayedLaunch();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StaffProjectile", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StaffProjectile", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StaffProjectile", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	// 비행 트레일 VFX. TrailVFX가 설정돼 있으면 BeginPlay에서 이 컴포넌트에 붙여 재생한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "StaffProjectile|VFX", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UNiagaraComponent> TrailVFXComponent;

	// 화살 꼬리 트레일 Niagara. 비워두면 트레일 없음(선택). BP/에셋에서 지정.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StaffProjectile|VFX")
	TObjectPtr<UNiagaraSystem> TrailVFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StaffProjectile|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	// 넉백 강도. 적중 시 URetrieveKnockbackLibrary::ApplyKnockbackFromSource로 적용.
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

	// 지연 발사(ArmDelayedLaunch) 상태
	FTimerHandle LaunchDelayTimerHandle;
	FVector PendingLaunchDirection = FVector::ZeroVector;
	float PendingLaunchSpeed = 0.f;
};
