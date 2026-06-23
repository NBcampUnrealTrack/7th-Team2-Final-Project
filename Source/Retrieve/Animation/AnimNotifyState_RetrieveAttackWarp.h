#pragma once

#include "AnimNotifyState_MotionWarping.h"
#include "AnimNotifyState_RetrieveAttackWarp.generated.h"

class UAnimSequenceBase;
class UMotionWarpingComponent;
class URootMotionModifier;

/**
 * 플레이어 공격용 모션워핑 노티.
 * 엔진 Motion Warping 노티를 상속해, 모디파이어 생성 직전(AddRootMotionModifier)에 워프 타겟을
 * '자동 해석'해서 등록한다 — 락온 우선 → 없으면 입력 방향 콘 검색. 도달 거리는 이 노티 윈도우의
 * 루트모션으로 클램프(애님이 곧 사거리). 적이 없으면 입력 방향으로 회전 + 그 거리만큼 전진.
 *
 * 플레이어 전용: 입력벡터/락온이 없는 폰(몬스터/AI)에 붙으면 타겟을 비워 조용히 통과한다(생 루트모션).
 * 몬스터는 스톡 Motion Warping 노티 + AI가 직접 타겟을 등록하는 방식을 쓸 것.
 */
UCLASS(meta = (DisplayName = "Motion Warping (Retrieve Attack)"))
class RETRIEVE_API UAnimNotifyState_RetrieveAttackWarp : public UAnimNotifyState_MotionWarping
{
	GENERATED_BODY()

public:
	virtual URootMotionModifier* AddRootMotionModifier_Implementation(UMotionWarpingComponent* MotionWarpingComp, const UAnimSequenceBase* Animation, float StartTime, float EndTime) const override;

protected:
	// 락온 없을 때 입력 방향 콘 검색 파라미터. (GA_Attack 기본값과 동일)
	UPROPERTY(EditAnywhere, Category = "Retrieve|Warp", meta = (ClampMin = "0.0"))
	float SearchRange = 350.f;

	UPROPERTY(EditAnywhere, Category = "Retrieve|Warp", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float SearchHalfAngle = 60.f;

	UPROPERTY(EditAnywhere, Category = "Retrieve|Warp", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RangeWeightRate = 0.25f;

	// 타겟에 너무 딱 붙지 않게 캡슐 합 + 이만큼 더 벌린다(cm).
	UPROPERTY(EditAnywhere, Category = "Retrieve|Warp", meta = (ClampMin = "0.0"))
	float StandoffOffset = 20.f;

	// 수직 차가 이 값을 넘는 적은 제외(닿을 수 없는 높이).
	UPROPERTY(EditAnywhere, Category = "Retrieve|Warp", meta = (ClampMin = "0.0"))
	float MaxVerticalDelta = 120.f;

private:
	// 타겟 자동 해석 → 워프 타겟 등록(불가하면 클리어). const: 엔진 시그니처에 맞춤.
	void ResolveAndRegisterWarpTarget(UMotionWarpingComponent* MotionWarpingComp, const UAnimSequenceBase* Animation, float StartTime, float EndTime) const;
};
