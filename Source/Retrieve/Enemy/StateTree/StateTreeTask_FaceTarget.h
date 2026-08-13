#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "StateTreeExecutionTypes.h"
#include "StateTreeTask_FaceTarget.generated.h"

class APawn;

USTRUCT(BlueprintType)
struct FStateTreeTask_FaceTargetInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta=(Optional))
	TObjectPtr<AActor> TargetPlayer = nullptr;

	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float FacingAcceptanceAngle = 8.f;

	/** 공격 회전보다 느리게 타깃을 바라보도록 사용하는 경계 상태용 보간 속도 */
	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.0"))
	float FacingInterpSpeed = 3.f;
};

/** 이동하지 않고 Suspicious 대상 방향으로 회전하는 Task */
USTRUCT(BlueprintType, meta = (DisplayName = "Face Target (No Move)", Category = "Retrieve|AI"))
struct RETRIEVE_API FStateTreeTask_FaceTarget : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTask_FaceTargetInstanceData;

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

private:
	TStateTreeExternalDataHandle<APawn> PawnHandle;
};
