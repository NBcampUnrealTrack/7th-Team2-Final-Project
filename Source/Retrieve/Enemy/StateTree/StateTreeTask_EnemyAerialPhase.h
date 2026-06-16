#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "StateTreeExecutionTypes.h"
#include "StateTreeTask_EnemyAerialPhase.generated.h"

class APawn;
class ARetrieveEnemyCharacter;
class UAnimSequenceBase;
class UEnemyCombatComponent;

USTRUCT(BlueprintType)
struct FStateTreeTask_EnemyAerialPhaseInstanceData
{
	GENERATED_BODY()

	/** Evaluator에서 바인딩: 추적 대상 플레이어 */
	UPROPERTY(EditAnywhere, meta=(Optional))
	TObjectPtr<AActor> TargetPlayer = nullptr;

	/** 플레이어 Z 기준 호버링 높이 (cm) */
	UPROPERTY(EditAnywhere, Category="Config", meta=(ClampMin="200.0"))
	float HoverHeight = 700.f;

	/** 호버링 유지 시간 (초). 이 시간이 지나면 Succeeded 반환 후 착지 */
	UPROPERTY(EditAnywhere, Category="Config", meta=(ClampMin="1.0"))
	float HoverDuration = 6.f;

	/** 목표 위치와의 허용 오차 (cm). 이 범위 안에 들어오면 도달로 판정 */
	UPROPERTY(EditAnywhere, Category="Config", meta=(ClampMin="10.0"))
	float PositionTolerance = 120.f;

	UPROPERTY(EditAnywhere, Category="Config", meta=(ClampMin="0.1"))
	float SpecialAttackRetryInterval = 0.75f;

	UPROPERTY(EditAnywhere, Category="Config", meta=(ClampMin="0.5"))
	float LandingTimeout = 4.f;

	UPROPERTY(EditAnywhere, Category="Animation")
	TSoftObjectPtr<UAnimSequenceBase> TakeOffAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/External/PolygonalCreaturesPack/PolygonalDragon/Animations/Polygonal_Dragon_AnimationFly_Up.Polygonal_Dragon_AnimationFly_Up")));

	UPROPERTY(EditAnywhere, Category="Animation")
	TSoftObjectPtr<UAnimSequenceBase> FlightAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/External/PolygonalCreaturesPack/PolygonalDragon/Animations/Polygonal_Dragon_AnimationFly_Forward.Polygonal_Dragon_AnimationFly_Forward")));

	UPROPERTY(EditAnywhere, Category="Animation")
	TSoftObjectPtr<UAnimSequenceBase> HoverAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/External/PolygonalCreaturesPack/PolygonalDragon/Animations/Polygonal_Dragon_AnimationFly_Idle.Polygonal_Dragon_AnimationFly_Idle")));

	UPROPERTY(EditAnywhere, Category="Animation")
	TSoftObjectPtr<UAnimSequenceBase> LandingAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/External/PolygonalCreaturesPack/PolygonalDragon/Animations/Polygonal_Dragon_AnimationFly_Down.Polygonal_Dragon_AnimationFly_Down")));

	UPROPERTY(EditAnywhere, Category="Animation", meta=(ClampMin="0.01"))
	float MontagePlayRate = 1.f;

	/** EnterState 시 캡처한 지면 Z 좌표 */
	float GroundZ = 0.f;

	/** 호버 위치 도달 여부 */
	bool bReachedHoverHeight = false;

	/** 호버 도달 후 경과 시간 */
	float ElapsedHoverTime = 0.f;

	/** 전체 경과 시간 (안전 타임아웃용) */
	float TotalElapsedTime = 0.f;

	bool bPlayedFlightMontage = false;

	bool bPlayedHoverMontage = false;

	bool bRequestedSpecialPattern = false;

	bool bLandingStarted = false;

	float LandingElapsedTime = 0.f;

	float TimeSinceSpecialAttackRequest = 0.f;

	UPROPERTY()
	TWeakObjectPtr<ARetrieveEnemyCharacter> CachedEnemy = nullptr;

	UPROPERTY()
	TWeakObjectPtr<UEnemyCombatComponent> CachedCombatComponent = nullptr;
};

/**
 * 에픽 몬스터(드래곤) 공중 호버링 태스크.
 * EnterState에서 MOVE_Flying으로 전환하여 플레이어 상공으로 상승 후 호버링,
 * HoverDuration 경과 후 Succeeded를 반환하고 ExitState에서 MOVE_Falling으로 복귀한다.
 */
USTRUCT(BlueprintType, meta=(DisplayName="Enemy Aerial Phase", Category="Retrieve|AI"))
struct RETRIEVE_API FStateTreeTask_EnemyAerialPhase : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTask_EnemyAerialPhaseInstanceData;

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

private:
	TStateTreeExternalDataHandle<APawn> PawnHandle;
};
