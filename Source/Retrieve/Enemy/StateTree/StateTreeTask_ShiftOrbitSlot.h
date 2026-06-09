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
	float StrafeInterval = 0.8f;

	UPROPERTY(EditAnywhere, Category = "Config")
	int32 StrafeDirection = 1;

	UPROPERTY(EditAnywhere, Category = "Config", meta=(ClampMin="1"))
	int32 MaxSlotShiftSteps = 2;

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
