#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "UObject/Class.h"
#include "StateTreeTask_LumenRetreat.generated.h"

class ULumenFollowComponent;

USTRUCT(BlueprintType)
struct FStateTreeTask_LumenRetreatInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.0"))
	float MoveAcceptableRadius = 80.f;

	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.0"))
	float MaxWaitForQuery = 2.f;

	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.0"))
	float RetreatSpeed = 650.f;

	float Elapsed = 0.f;
	bool bMoveIssued = false;

	UPROPERTY()
	TWeakObjectPtr<ULumenFollowComponent> CachedComp = nullptr;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Lumen Retreat To Safe Spot", Category = "Retrieve|AI"))
struct RETRIEVE_API FStateTreeTask_LumenRetreat : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTask_LumenRetreatInstanceData;

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
	                                       const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

private:
	TStateTreeExternalDataHandle<APawn> PawnHandle;
};
