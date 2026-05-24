#include "Enemy/StateTree/StateTreeTask_EnemyAttack.h"

#include "StateTreeLinker.h"
#include "StateTreeExecutionContext.h"
#include "Components/EnemyCombatComponent.h"

bool FStateTreeTask_EnemyAttack::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(PawnHandle);
	return true;
}

EStateTreeRunStatus FStateTreeTask_EnemyAttack::EnterState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime = 0.f;

	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	UEnemyCombatComponent* Combat = Pawn->FindComponentByClass<UEnemyCombatComponent>();
	if (!Combat || !Combat->RequestBasicAttack(InstanceData.TargetActor))
	{
		return EStateTreeRunStatus::Failed;
	}
	
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_EnemyAttack::Tick(
	FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime += DeltaTime;
	
	/*APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	if (!Pawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemyAttackTask Failed: Pawn missing"));
		return EStateTreeRunStatus::Failed;
	}
	
	UEnemyCombatComponent* Combat = Pawn->FindComponentByClass<UEnemyCombatComponent>();
	if (!Combat || !Combat->IsPatternActive())
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemyAttackTask Succeeded: PatternInactive"));
		return EStateTreeRunStatus::Succeeded;
	}*/

	if (InstanceData.ElapsedTime >= InstanceData.MaxAttackDuration)
	{
		UE_LOG(LogTemp, Warning, TEXT("EnemyAttackTask Succeeded: MaxAttackDuration"));
		return EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;
}

void FStateTreeTask_EnemyAttack::ExitState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime = 0.f;
	
	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	if (!Pawn)
	{
		return;
	}
	
	if (UEnemyCombatComponent* Combat = Pawn->FindComponentByClass<UEnemyCombatComponent>())
	{
		Combat->StopCurrentPattern();
	}
}
