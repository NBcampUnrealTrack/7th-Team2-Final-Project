#include "StateTreeTask_LumenHold.h"

#include "AIController.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "Lumen/LumenFollowComponent.h"

bool FStateTreeTask_LumenHold::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(PawnHandle);
	return true;
}

EStateTreeRunStatus FStateTreeTask_LumenHold::EnterState(FStateTreeExecutionContext& Context,
                                                         const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.CachedComp = Pawn->FindComponentByClass<ULumenFollowComponent>();
	if (InstanceData.CachedComp.IsValid())
	{
		InstanceData.CachedComp->SetModeFromStateTree(InstanceData.HoldMode);
	}

	if (AAIController* AIController = Pawn->GetController<AAIController>())
	{
		AIController->StopMovement();
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_LumenHold::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	// 제자리 유지; 대기 해제, 전투 종료, 재쿼리 등의 전환으로 상태가 종료됨 
	return EStateTreeRunStatus::Running;
}
