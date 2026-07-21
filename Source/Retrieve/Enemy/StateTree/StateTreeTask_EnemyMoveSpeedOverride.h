#pragma once

#include "CoreMinimal.h"
#include "StateTreeExecutionTypes.h"
#include "StateTreeTaskBase.h"
#include "StateTreeTask_EnemyMoveSpeedOverride.generated.h"

USTRUCT()
struct FStateTreeTask_EnemyMoveSpeedOverrideInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.0"))
	float MoveSpeed = 400.f;

	float PreviousMaxWalkSpeed = 0.f;
	bool bApplied = false;
};

USTRUCT(meta = (DisplayName = "Enemy Move Speed Override"))
struct RETRIEVE_API FStateTreeTask_EnemyMoveSpeedOverride : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTask_EnemyMoveSpeedOverrideInstanceData;

	FStateTreeTask_EnemyMoveSpeedOverride();

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;

	virtual void ExitState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;

private:
	TStateTreeExternalDataHandle<APawn> PawnHandle;
};
