#include "StateTreeTask_EnemyMoveSpeedOverride.h"

#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "Character/RetrieveEnemyCharacter.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

FStateTreeTask_EnemyMoveSpeedOverride::FStateTreeTask_EnemyMoveSpeedOverride()
{
	bShouldCallTick = false;
}

bool FStateTreeTask_EnemyMoveSpeedOverride::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(PawnHandle);
	return true;
}

EStateTreeRunStatus FStateTreeTask_EnemyMoveSpeedOverride::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	InstanceData.bApplied = false;

	ACharacter* Character = Cast<ACharacter>(Context.GetExternalDataPtr(PawnHandle));
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;

	if (!Movement)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.PreviousMaxWalkSpeed = Movement->MaxWalkSpeed;
	Movement->MaxWalkSpeed = InstanceData.MoveSpeed;
	InstanceData.bApplied = true;

	// MoveTo Task가 상태 완료를 담당합니다.
	return EStateTreeRunStatus::Running;
}

void FStateTreeTask_EnemyMoveSpeedOverride::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	if (!InstanceData.bApplied)
	{
		return;
	}

	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);

	if (ARetrieveEnemyCharacter* Enemy = Cast<ARetrieveEnemyCharacter>(Pawn))
	{
		// 현재 GAS MoveSpeed 값을 다시 적용합니다.
		Enemy->RefreshMoveSpeedFromAttribute();
	}
	else if (ACharacter* Character = Cast<ACharacter>(Pawn))
	{
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->MaxWalkSpeed = InstanceData.PreviousMaxWalkSpeed;
		}
	}

	InstanceData.bApplied = false;
}
