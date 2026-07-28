#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Enemy/EnemyProjectile.h"
#include "EpicEnemyProjectile.generated.h"

/**
 * 에픽 몬스터 전용 투사체.
 * 공통 AEnemyProjectile의 발사/유도/VFX 동작을 그대로 상속하고,
 * 에픽 전용 패링 반사(카운터) 로직만 추가한다.
 * BP_LavaProjectile / BP_WaterProjectile 등 에픽 투사체 에셋이 이 클래스를 상속한다.
 */
UCLASS(Blueprintable, BlueprintType)
class RETRIEVE_API AEpicEnemyProjectile : public AEnemyProjectile
{
	GENERATED_BODY()

public:
	/** 패링 시 카운터 타겟(보통 발사한 몬스터)으로 반사되도록 설정한다. */
	UFUNCTION(BlueprintCallable, Category="EpicEnemyProjectile|Reflection")
	void ConfigureReflection(AActor* InCounterTarget, float InReflectedSpeedMultiplier = 1.2f);

protected:
	virtual bool HandleReflectedOverlap(AActor* OtherActor, const FHitResult& SweepResult) override;
	virtual bool TryReflectOnHit(AActor* OtherActor, UAbilitySystemComponent* OtherASC) override;
	virtual bool IsIgnoredActor(const AActor* OtherActor) const override;
	// 에픽 투사체는 충돌 정지 시 반경 AoE 데미지를 적용한다.
	virtual bool ShouldApplyImpactRadiusDamage() const override { return true; }

private:
	void ReflectTowardCounterTarget(AActor* ParryingActor);
	bool TryApplyReflectedCounter(AActor* OtherActor);

	UPROPERTY(Transient)
	TObjectPtr<AActor> ReflectedCounterTarget;

	UPROPERTY(Transient)
	TObjectPtr<AActor> ReflectedInstigator;

	bool bReflectable = false;
	bool bReflected = false;
	float ReflectedSpeedMultiplier = 1.2f;
};
