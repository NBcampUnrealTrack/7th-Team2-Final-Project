#include "StateTreeTask_LumenRetreat.h"

#include "AIController.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Lumen/LumenFollowComponent.h"

bool FStateTreeTask_LumenRetreat::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(PawnHandle);
	return true;
}

EStateTreeRunStatus FStateTreeTask_LumenRetreat::EnterState(FStateTreeExecutionContext& Context,
                                                            const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.CachedComp = Pawn->FindComponentByClass<ULumenFollowComponent>();
	if (!InstanceData.CachedComp.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	AAIController* AIController = Pawn->GetController<AAIController>();

	// 비동기 EQS 대기 시간 동안 Lumen이 호스트를 계속 추적(전투로 진입)하지 않도록 MoveToActor(host)를 취소
	if (AIController)
	{
		AIController->StopMovement();
	}

	// 도주 속도 확정 (Follow의 밴딩으로 인해 MaxWalkSpeed가 남아있을 수 있음)
	if (ACharacter* LumenChar = Cast<ACharacter>(Pawn))
	{
		if (UCharacterMovementComponent* Move = LumenChar->GetCharacterMovement())
		{
			Move->MaxWalkSpeed = InstanceData.RetreatSpeed;
		}
	}

	InstanceData.CachedComp->SetModeFromStateTree(EFollowMode::RetreatCombat);
	InstanceData.CachedComp->RequestSafeSpotQuery();
	InstanceData.Elapsed = 0.f;
	InstanceData.bMoveIssued = false;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_LumenRetreat::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.Elapsed += DeltaTime;

	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	ULumenFollowComponent* Comp = InstanceData.CachedComp.Get();
	if (!Pawn || !Comp)
	{
		return EStateTreeRunStatus::Failed;
	}

	AAIController* AIController = Pawn->GetController<AAIController>();
	if (!AIController)
	{
		return EStateTreeRunStatus::Failed;
	}

	// 쿼리가 아직 진행 중인가?
	if (!Comp->HasValidSafeSpot())
	{
		if (InstanceData.Elapsed >= InstanceData.MaxWaitForQuery)
		{
			// 폴백: 서 있는 곳에서 웅크림
			AIController->StopMovement();
			return EStateTreeRunStatus::Succeeded;
		}
		return EStateTreeRunStatus::Running;
	}

	const FVector Spot = Comp->GetSafeSpot();

	if (!InstanceData.bMoveIssued)
	{
		AIController->MoveToLocation(Spot, InstanceData.MoveAcceptableRadius, true, true, true, false, nullptr, true);
		InstanceData.bMoveIssued = true;
	}

	if (FVector::Dist(Pawn->GetActorLocation(), Spot) <= InstanceData.MoveAcceptableRadius)
	{
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}
