#include "Enemy/StateTree/StateTreeTask_EnemyPatternAttack.h"

#include "StateTreeLinker.h"
#include "StateTreeExecutionContext.h"
#include "Components/EnemyCombatComponent.h"
#include "Enemy/EncirclementSubsystem.h"
#include "Logging/RetrieveLogChannels.h"

bool FStateTreeTask_EnemyPatternAttack::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(PawnHandle);
	return true;
}

EStateTreeRunStatus FStateTreeTask_EnemyPatternAttack::EnterState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime = 0.f;
	InstanceData.TimeSinceAttackRequested = 0.f;

	InstanceData.bStartAttack = false;
	InstanceData.bObservedPatternActive = false;
	
	if (!IsValid(InstanceData.TargetActor))
	{
		return EStateTreeRunStatus::Failed;
	}

	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.CachedCombatComponent = Pawn->FindComponentByClass<UEnemyCombatComponent>();
	if (!InstanceData.CachedCombatComponent.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	UEncirclementSubsystem* EncircleSubsystem = Pawn->GetWorld()->GetSubsystem<UEncirclementSubsystem>();
	const bool bTokenRequested =
	EncircleSubsystem && EncircleSubsystem->RequestAttackToken(InstanceData.TargetActor, Pawn);

	if (!bTokenRequested)
	{
		return EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_EnemyPatternAttack::Tick(
	FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime += DeltaTime;
	if (InstanceData.bStartAttack)
	{
		InstanceData.TimeSinceAttackRequested += DeltaTime;
	}

	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!InstanceData.bStartAttack)
	{
		if (!InstanceData.CachedCombatComponent.IsValid())
		{
			return EStateTreeRunStatus::Failed;
		}

		const bool bRequested =
			InstanceData.CachedCombatComponent->RequestPatternByPriority(InstanceData.TargetActor);

		if (!bRequested)
		{
			return EStateTreeRunStatus::Failed;
		}

		InstanceData.bStartAttack = true;
		InstanceData.bObservedPatternActive = false;
		InstanceData.TimeSinceAttackRequested = 0.f;
		return EStateTreeRunStatus::Running;
	}
	else if (InstanceData.bStartAttack)
	{
		if (!InstanceData.CachedCombatComponent.IsValid())
		{
			return EStateTreeRunStatus::Failed;
		}

		const bool bPatternActive = InstanceData.CachedCombatComponent->IsPatternActive();
		
		UE_LOG(LogRetrieveCombat, Warning,
		TEXT("[EnemyPatternAttack] Tick Active=%d Observed=%d TimeSinceRequest=%.2f"),
		bPatternActive,
		InstanceData.bObservedPatternActive,
		InstanceData.TimeSinceAttackRequested);
		
		if (bPatternActive)
		{
			InstanceData.bObservedPatternActive = true;
		}
		else if (InstanceData.bObservedPatternActive)
		{
			return EStateTreeRunStatus::Succeeded;
		}
		else if (InstanceData.TimeSinceAttackRequested >= InstanceData.AttackStartGraceTime)
		{
			return EStateTreeRunStatus::Failed;
		}
	}

	if (InstanceData.ElapsedTime >= InstanceData.MaxAttackDuration)
	{
		return EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;
}

void FStateTreeTask_EnemyPatternAttack::ExitState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime = 0.f;
	InstanceData.TimeSinceAttackRequested = 0.f;

	InstanceData.bStartAttack = false;
	InstanceData.bObservedPatternActive = false;

	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	if (!Pawn)
	{
		return;
	}

	if (UEnemyCombatComponent* Combat = Pawn->FindComponentByClass<UEnemyCombatComponent>())
	{
		Combat->StopCurrentPattern();
	}

	if (UEncirclementSubsystem* Enc = Pawn->GetWorld()->GetSubsystem<UEncirclementSubsystem>())
	{
		Enc->ReleaseAttackToken(InstanceData.TargetActor, Pawn);
	}
}
