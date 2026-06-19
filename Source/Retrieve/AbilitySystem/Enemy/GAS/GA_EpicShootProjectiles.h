#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Enemy/GAS/GA_ShootProjectiles.h"
#include "GA_EpicShootProjectiles.generated.h"

class ACharacter;
class UAnimSequenceBase;

/**
 * 에픽 몬스터 전용 투사체 특수공격 GA.
 * 공통 UGA_ShootProjectiles의 데이터 구동 스폰 로직(Aimed/Rain/Spread)을 그대로 상속하고,
 * 에픽 전용 동작(타겟 조준, 공중 모드 진입, 투사체 반사 설정, 대체 시퀀스 몽타주)만 추가한다.
 * GA_Dragon_AerialFireball 등 에픽 GA 에셋이 이 클래스를 상속한다.
 *
 * 새 패턴은 MonsterPattern row의 SpawnPattern/투사체 설정과 VFX만으로 추가 가능하며,
 * 이 클래스를 수정할 필요가 없다.
 */
UCLASS(Blueprintable, BlueprintType)
class RETRIEVE_API UGA_EpicShootProjectiles : public UGA_ShootProjectiles
{
	GENERATED_BODY()

protected:
	virtual UAnimMontage* ResolveFallbackSequenceMontage() const override;
	virtual void OnSpecialAttackActivated() override;
	virtual void OnSpecialAttackEnded() override;
	virtual void OnBeforeProjectileSpawn() override;
	virtual void OnProjectileSpawned(AEnemyProjectile* Projectile, AActor* AvatarActor) override;

	/** 매칭 몽타주가 없을 때 동적 몽타주로 재생할 대체 시퀀스 (예: 드래곤 공중 발사 애니). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Epic|ShootProjectiles")
	TSoftObjectPtr<UAnimSequenceBase> FallbackMontageSequence;

	/** Activate 시 공중 모드로 진입할지 여부 (비행 에픽만 true). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Epic|ShootProjectiles")
	bool bUseAerialModeOnActivate = false;

	/** 공중 모드 진입 시 들어올릴 높이 (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Epic|ShootProjectiles", meta=(ClampMin="0.0"))
	float AerialLiftHeight = 350.f;

private:
	void FaceCachedTarget() const;

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> CachedAvatarCharacter;

	bool bAerialModeApplied = false;
};
