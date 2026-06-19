#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "UObject/Class.h"
#include "StateTreeTask_LumenHold.generated.h"

class ULumenFollowComponent;

USTRUCT(BlueprintType)
struct FStateTreeTask_LumenHoldInstanceData
{
	GENERATED_BODY()

	// Mode: Wait (Idle, 대기) 또는 RetreatCombat (안전 지점에서 웅크리기).
	UPROPERTY(EditAnywhere, Category = "Config")
	EFollowMode HoldMode = EFollowMode::Wait;

	UPROPERTY()
	TWeakObjectPtr<ULumenFollowComponent> CachedComp = nullptr;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Lumen Hold", Category = "Retrieve|AI"))
struct RETRIEVE_API FStateTreeTask_LumenHold : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTask_LumenHoldInstanceData;

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
	                                       const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

private:
	TStateTreeExternalDataHandle<APawn> PawnHandle;
};
