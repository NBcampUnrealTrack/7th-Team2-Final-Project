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

	// Attack류(FacingInterpSpeed=8)보다 느리게 — 경계 중 서서히 돌아보는 느낌을 위함
	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.0"))
	float FacingInterpSpeed = 3.f;
};

// 이동 없이 타겟을 향해 서서히 회전만 시키는 Task. Suspicious(경계) 상태에서 사용.
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
