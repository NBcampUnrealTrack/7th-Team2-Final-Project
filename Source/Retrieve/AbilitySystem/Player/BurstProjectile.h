#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Data/RetrieveDataTableTypes.h"
#include "BurstProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;

/**
 * 버스트 스킬의 투사체 (검기) 베이스 액터.
 *
 * PlayerBurstComponent::DoProjectileHit 에서 SpawnActorDeferred 로 생성한 뒤
 * Initialize(HitData, Instigator) → FinishSpawning → Launch(Direction, Speed) 순으로 호출한다.
 *
 * Overlap 이 발생하면 Instigator 의 PlayerBurstComponent::ApplyHitToTarget 으로 적중을 보고한다.
 */
UCLASS()
class RETRIEVE_API ABurstProjectile : public AActor
{
	GENERATED_BODY()

public:
	ABurstProjectile();

	/** SpawnActorDeferred 직후 호출. 발사 전 데이터 보장. */
	UFUNCTION(BlueprintCallable, Category = "Burst|Projectile")
	void Initialize(const FBurstHitInstance& InHitData, AActor* InInstigator);

	/** FinishSpawning 이후 호출. 발사 방향과 속도 적용. */
	UFUNCTION(BlueprintCallable, Category = "Burst|Projectile")
	void Launch(const FVector& Direction, float Speed);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnSphereOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

private:
	/** 콜리전 + 루트. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Burst|Projectile", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USphereComponent> CollisionComp;

	/** 직선 이동. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Burst|Projectile", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UProjectileMovementComponent> MovementComp;

	/** 자체 LifeTime(초). 0 이하면 무제한. BeginPlay 에서 SetLifeSpan 호출. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Burst|Projectile", meta = (ClampMin = "0.0", AllowPrivateAccess = "true"))
	float LifeTime = 3.f;

	/** 첫 적중 후 즉시 Destroy 할지. 기본 false = 관통 허용. BP 에서 끄면 단발성 투사체. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Burst|Projectile", meta = (AllowPrivateAccess = "true"))
	bool bDestroyOnFirstHit = false;

	/** Initialize 에서 저장된 스킬 데이터. ApplyHitToTarget 에 전달. */
	FBurstHitInstance HitData;

	/** 발사 주체. PlayerBurstComponent 를 가진 액터. */
	TWeakObjectPtr<AActor> InstigatorActor;

	/** 중복 적중 방지(관통 시 같은 적이 한 번만 맞도록). */
	TSet<TWeakObjectPtr<AActor>> AlreadyHitActors;
};
