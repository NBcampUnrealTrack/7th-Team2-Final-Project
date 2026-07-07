#pragma once

#include "CoreMinimal.h"
#include "Character/RetrieveEnemyCharacter.h"
#include "RetrieveEpicMonsterCharacter.generated.h"

class UAnimMontage;
class UAnimSequenceBase;
class UEpicMonsterGroggyComponent;

/**
 * 에픽 몬스터(드래곤/나무/바위 등) 전용 베이스 캐릭터.
 * 일반/보스 몬스터에 영향을 주지 않도록 에픽 전용 이동/회전/턴 애니메이션 로직을 이 클래스에 격리한다.
 *
 * - 전진 로코모션(bOrientRotationToMovement) 기반 이동 설정 적용
 * - 제자리 회전 시 그라운드 턴 애니메이션(좌/우) 재생
 *
 * 모든 수치/애니메이션 에셋은 BP에서 조정 가능하도록 EditDefaultsOnly 로 노출한다 (밸런싱 전용).
 */
UCLASS()
class RETRIEVE_API ARetrieveEpicMonsterCharacter : public ARetrieveEnemyCharacter
{
	GENERATED_BODY()

public:
	ARetrieveEpicMonsterCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void Tick(float DeltaSeconds) override;

	/** 전진 로코모션 사용 여부. true면 슬롯 이동 시 포커스 대신 이동 방향으로 회전한다. */
	virtual bool UsesForwardLocomotion() const override { return bUseForwardLocomotion; }

	/** 제자리 회전 턴 애니메이션 재생/갱신. SignedYawDelta>0이면 우회전, <0이면 좌회전. */
	virtual void UpdateGroundTurnAnimation(float SignedYawDelta) override;

	/** 외부(StateTree 태스크)에서 턴 애니메이션을 즉시 정지할 때 호출. */
	virtual void StopGroundTurnAnimation() override { StopLocomotionMontages(); }

	/** 에픽은 비행 진입용으로 공중에 배치/스폰될 수 있으므로, 스폰 시 네비메시(지면)로 스냅한다. */
	virtual bool ShouldGroundSnapOnSpawn() const override { return true; }
	virtual bool ShouldUseStateTreeAerialPhase() const override { return false; }
	virtual bool ShouldUseDirectChaseToTarget() const override { return true; }
	virtual bool ShouldFaceTargetDuringShiftOrbit() const override { return true; }
	virtual bool ShouldSuppressNormalAttackWhileFlying() const override { return true; }
	virtual bool ShouldUsePatternRangeForNormalAttack() const override { return true; }
	virtual bool ShouldUse2DPatternRangeWhileFlying() const override { return true; }
	virtual bool ShouldTreatZeroPatternMaxRangeAsUnlimited() const override { return true; }
	virtual bool ShouldUseFallbackEpicStateTree() const override { return true; }
	virtual float GetHorizontalHalfFOVOverrideForAI() const override { return HorizontalHalfFOVOverride; }
	virtual float GetPeripheralVisionAngleOverrideForAI() const override { return PeripheralVisionAngleOverride; }

protected:
	virtual void ConfigureEnemyMovement() override;
	virtual void StopLocomotionMontages() override;

	// ---- 이동/회전 설정 (밸런싱용) ----

	/** 전진 로코모션(bOrientRotationToMovement) 사용 여부 */
	UPROPERTY(EditDefaultsOnly, Category="Epic|Movement")
	bool bUseForwardLocomotion = true;

	UPROPERTY(EditDefaultsOnly, Category="Epic|Movement", meta=(ClampMin="0.0"))
	float RotationRateYaw = 180.f;

	UPROPERTY(EditDefaultsOnly, Category="Epic|Movement", meta=(ClampMin="0.0"))
	float MaxAcceleration = 900.f;

	UPROPERTY(EditDefaultsOnly, Category="Epic|Movement", meta=(ClampMin="0.0"))
	float BrakingDeceleration = 900.f;

	UPROPERTY(EditDefaultsOnly, Category="Epic|Movement", meta=(ClampMin="0.0"))
	float GroundFriction = 6.f;

	/** RVO Avoidance 사용 여부 (에픽은 보통 끈다) */
	UPROPERTY(EditDefaultsOnly, Category="Epic|Movement")
	bool bUseRVOAvoidance = false;

	/** >0이면 캡슐 크기를 재설정 (Radius) */
	UPROPERTY(EditDefaultsOnly, Category="Epic|Movement", meta=(ClampMin="0.0"))
	float CapsuleRadius = 0.f;

	UPROPERTY(EditDefaultsOnly, Category="Epic|Movement", meta=(ClampMin="0.0"))
	float CapsuleHalfHeight = 0.f;

	/** true면 캡슐 변경과 함께 메시 상대 Z 위치를 MeshRelativeZ로 보정 */
	UPROPERTY(EditDefaultsOnly, Category="Epic|Movement")
	bool bOverrideMeshRelativeZ = false;

	UPROPERTY(EditDefaultsOnly, Category="Epic|Movement")
	float MeshRelativeZ = 0.f;

	/** >0이면 체력바 Z 위치를 이 값(cm)으로 오버라이드. 기본(120)은 일반 몬스터 기준이므로 대형 몬스터는 이 값을 조정 */
	UPROPERTY(EditDefaultsOnly, Category="Epic|Movement", meta=(ClampMin="0.0"))
	float HealthBarHeightOffset = 0.f;

	// ---- 그라운드 턴 애니메이션 (밸런싱/연출용) ----

	/** 우회전 시 재생할 시퀀스. 좌/우 둘 다 비어 있으면 턴 애니메이션을 사용하지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category="Epic|TurnAnimation")
	TSoftObjectPtr<UAnimSequenceBase> TurnRightAnimation;

	UPROPERTY(EditDefaultsOnly, Category="Epic|TurnAnimation")
	TSoftObjectPtr<UAnimSequenceBase> TurnLeftAnimation;

	/** 이 각도(deg) 미만의 회전에서는 턴 애니메이션을 재생하지 않는다. */
	UPROPERTY(EditDefaultsOnly, Category="Epic|TurnAnimation", meta=(ClampMin="0.0"))
	float TurnAnimMinYaw = 18.f;

	/** 이 속도(cm/s) 초과로 이동 중이면 턴 애니메이션을 재생하지 않는다 (이동 중엔 로코모션이 처리). */
	UPROPERTY(EditDefaultsOnly, Category="Epic|TurnAnimation", meta=(ClampMin="0.0"))
	float TurnAnimMovingSpeedThreshold = 80.f;

	UPROPERTY(EditDefaultsOnly, Category="Epic|TurnAnimation")
	FName TurnAnimationSlot = FName(TEXT("DefaultSlot"));

	UPROPERTY(EditDefaultsOnly, Category="Epic|Locomotion")
	TSoftObjectPtr<UAnimSequenceBase> ForcedGroundMoveAnimation;

	UPROPERTY(EditDefaultsOnly, Category="Epic|Locomotion", meta=(ClampMin="0.1"))
	float ForcedGroundMovePlayRate = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category="Epic|Locomotion", meta=(ClampMin="0.0"))
	float AnimMovingSpeedThreshold = 10.f;

	UPROPERTY(EditDefaultsOnly, Category="Epic|AI", meta=(ClampMin="0.0", ClampMax="180.0"))
	float HorizontalHalfFOVOverride = 120.f;

	UPROPERTY(EditDefaultsOnly, Category="Epic|AI|Perception", meta=(ClampMin="0.0", ClampMax="180.0"))
	float PeripheralVisionAngleOverride = 170.f;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UEpicMonsterGroggyComponent> EpicGroggyComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> GroundTurnMontage = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> GroundMoveMontage = nullptr;

	int32 GroundTurnDirection = 0;

	bool bMovementDrivenChaseTagAdded = false;
	FVector LastAnimMovementSampleLocation = FVector::ZeroVector;
	bool bHasAnimMovementSampleLocation = false;
};
