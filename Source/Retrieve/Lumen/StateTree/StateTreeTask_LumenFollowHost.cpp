#include "StateTreeTask_LumenFollowHost.h"

#include "AIController.h"
#include "Navigation/PathFollowingComponent.h" // EPathFollowingStatus
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Lumen/LumenFollowComponent.h"

bool FStateTreeTask_LumenFollowHost::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(PawnHandle);
	return true;
}

EStateTreeRunStatus FStateTreeTask_LumenFollowHost::EnterState(FStateTreeExecutionContext& Context,
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

	InstanceData.CachedComp->SetModeFromStateTree(EFollowMode::Follow);
	InstanceData.StuckTime = 0.f;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_LumenFollowHost::Tick(FStateTreeExecutionContext& Context,
                                                         const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	ULumenFollowComponent* Comp = InstanceData.CachedComp.Get();
	AActor* Host = InstanceData.HostActor;
	if (!Pawn || !Comp || !Host)
	{
		return EStateTreeRunStatus::Running;
	}

	AAIController* AIController = Pawn->GetController<AAIController>();
	if (!AIController)
	{
		return EStateTreeRunStatus::Running;
	}

	const float Distance = InstanceData.DistanceToHost;
	const float TeleportDistance = Comp->GetTeleportDistance();
	const float StopRadius = Comp->GetFollowDistance();
	const float StartRadius = StopRadius + InstanceData.FollowBandWidth;

	const bool bMoving = (AIController->GetMoveStatus() == EPathFollowingStatus::Moving);
	const bool bShouldMove = (Distance > StartRadius);

	InstanceData.StuckTime = (bShouldMove && !bMoving) ? (InstanceData.StuckTime + DeltaTime) : 0.f;

	const bool bRecover = (TeleportDistance > 0.f && Distance > TeleportDistance) || (InstanceData.StuckTime >
		InstanceData.StuckRecoverSeconds);
	if (bRecover)
	{
		const FVector Landing = Host->GetActorLocation() + ULumenFollowComponent::ComputeBehindLeftOffset(
			Host, Comp->GetOffsetBack(), Comp->GetOffsetLeft());
		AIController->StopMovement();
		Pawn->SetActorLocation(Landing, false);
		const float Yaw = (Host->GetActorLocation() - Landing).Rotation().Yaw;
		Pawn->SetActorRotation(FRotator(0.f, Yaw, 0.f));
		InstanceData.StuckTime = 0.f;
		return EStateTreeRunStatus::Running;
	}

	if (ACharacter* LumenChar = Cast<ACharacter>(Pawn))
	{
		if (UCharacterMovementComponent* Move = LumenChar->GetCharacterMovement())
		{
			float DesiredSpeed = InstanceData.WalkSpeed;
			if (Distance > InstanceData.SprintBandDistance) { DesiredSpeed = InstanceData.SprintSpeed; }
			else if (Distance > InstanceData.JogBandDistance) { DesiredSpeed = InstanceData.JogSpeed; }

			Move->MaxWalkSpeed = FMath::FInterpTo(Move->MaxWalkSpeed, DesiredSpeed, DeltaTime,
			                                      InstanceData.SpeedInterpRate);
		}
	}

	if (!bMoving && bShouldMove)
	{
		AIController->MoveToActor(Host, StopRadius, false, true, true, nullptr, true);
	}

	return EStateTreeRunStatus::Running;
}
