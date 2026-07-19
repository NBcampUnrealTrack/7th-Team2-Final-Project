#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "StateTreeExecutionTypes.h"
#include "StateTreeTask_ShiftOrbitSlot.generated.h"

class APawn;

USTRUCT(BlueprintType)
struct FStateTreeTask_ShiftOrbitSlotInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta=(Optional))
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(EditAnywhere, Category = "Config")
	float StrafeInterval = 2.5f;

	UPROPERTY(EditAnywhere, Category = "Config")
	float StrafeIntervalJitter = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Config")
	int32 StrafeDirection = 1;

	UPROPERTY(EditAnywhere, Category = "Config", meta=(ClampMin="1"))
	int32 MaxSlotShiftSteps = 1;
	
	/** 링을 점유하는 적이 해당 값 미만이면 서클링을 생략합니다(1:1 가디언 등은 큰 값으로 설정합니다.)
	 *  StateTree의 Combat.Strafe 진입 여부와는 별개이며, 이 값은 Strafe 상태 유지 중
	 *  슬롯 로테이션 실행 여부만 결정합니다. */
	UPROPERTY(EditAnywhere, Category = "Config", meta=(ClampMin="1"))
	int32 MinOccupantsToCircle = 1;

	/** 실제 이동 목적지. Evaluator의 ChaseLocation을 바인딩한다.
	 *  bWaitForArrivalBeforeShift=true일 때만 참조됨. */
	UPROPERTY(EditAnywhere, Category = "Input")
	FVector ChaseLocation = FVector::ZeroVector;

	/** 슬롯 목적지에 도착했다고 판단할 반경.
	 *  bWaitForArrivalBeforeShift=true일 때만 사용. */
	UPROPERTY(EditAnywhere, Category = "Config", meta=(ClampMin="0.0"))
	float ArrivalRadius = 80.f;

	/** true면 도착 후에만 StrafeInterval을 누적하고 다음 슬롯으로 넘어간다.
	 *  false면 기존 동작 완전 유지(이동 중에도 누적, 도착 판정 없이 만료 시 슬롯 이동).
	 *  Normal.Strafe에서만 opt-in. Bow/Boss/Epic은 MoveTo 실효 반경 미검증이라 기본 false. */
	UPROPERTY(EditAnywhere, Category = "Config")
	bool bWaitForArrivalBeforeShift = false;

	float ElapsedTime = 0.f;
	bool bOriginalOrient = false;
	bool bOriginalControllerRot = false;
	bool bOriginalUseControllerRotationYaw = false;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Shift Orbit Slot", Category = "Retrieve|AI"))
struct RETRIEVE_API FStateTreeTask_ShiftOrbitSlot : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTask_ShiftOrbitSlotInstanceData;

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

private:
	TStateTreeExternalDataHandle<APawn> PawnHandle;
};
