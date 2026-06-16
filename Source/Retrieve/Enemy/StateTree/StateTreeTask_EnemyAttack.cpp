#include "Enemy/StateTree/StateTreeTask_EnemyAttack.h"

#include "StateTreeLinker.h"
#include "StateTreeExecutionContext.h"
#include "Components/Enemy/EnemyCombatComponent.h"
#include "Enemy/EncirclementSubsystem.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"

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
	InstanceData.TimeInSoftAttackRange = 0.f;
	InstanceData.TimeSinceAttackRequested = 0.f;

	InstanceData.bStartAttack = false;
	InstanceData.bObservedPatternActive = false;

	if (!IsValid(InstanceData.TargetPlayer))
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

	if (!InstanceData.CachedCombatComponent->IsAttackable(InstanceData.TargetPlayer))
	{
		return EStateTreeRunStatus::Failed;
	}
	
	UEncirclementSubsystem* EncircleSubsystem = Pawn->GetWorld()->GetSubsystem<UEncirclementSubsystem>();
	if (!EncircleSubsystem || !EncircleSubsystem->RequestAttackToken(InstanceData.TargetPlayer, Pawn))
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
		const float AttackStartRange = InstanceData.AttackRange + InstanceData.AttackStartRangeTolerance;
		if (InstanceData.DistanceToTarget > AttackStartRange)
		{
			InstanceData.TimeInSoftAttackRange = 0.f;
			
			if (AAIController* AIC = Pawn->GetController<AAIController>())
			{
				const bool bCanMove = InstanceData.CachedCombatComponent.IsValid()
					&& !InstanceData.CachedCombatComponent->IsMovementLockedByAttack();

				if (bCanMove && !InstanceData.ChaseLocation.IsNearlyZero())
				{
					const float MoveDeltaSq = FVector::DistSquared2D(
						InstanceData.LastMoveRequestLocation,
						InstanceData.ChaseLocation);

					if (InstanceData.LastMoveRequestLocation.IsNearlyZero() 
						|| MoveDeltaSq > FMath::Square(50.f))
					{
						AIC->MoveToLocation(
							InstanceData.ChaseLocation,
							InstanceData.MoveAcceptableRadius,
							true,
							true,
							true,
							true);

						InstanceData.LastMoveRequestLocation = InstanceData.ChaseLocation;
					}
				}
			}
			
			return EStateTreeRunStatus::Running;
		}

		if (InstanceData.DistanceToTarget > InstanceData.AttackRange)
		{
			InstanceData.TimeInSoftAttackRange += DeltaTime;
			if (InstanceData.TimeInSoftAttackRange < InstanceData.AttackStartDelay)
			{
				return EStateTreeRunStatus::Running;
			}
		}

		{
			if (!InstanceData.CachedCombatComponent.IsValid())
			{
				return EStateTreeRunStatus::Failed;
			}

			if (!InstanceData.CachedCombatComponent->IsAttackable(InstanceData.TargetPlayer))
			{
				return EStateTreeRunStatus::Failed;
			}

			InstanceData.bStartAttack = true;
			InstanceData.bObservedPatternActive = false;
			InstanceData.TimeSinceAttackRequested = 0.f;
			
			if (AAIController* AIC = Pawn->GetController<AAIController>())
			{
				AIC->StopMovement();
			
				if (UPathFollowingComponent* PathFollowing = AIC->GetPathFollowingComponent())
				{
					PathFollowing->AbortMove(*AIC, FPathFollowingResultFlags::ForcedScript);
				}
			}
			
			if (ACharacter* Character = Cast<ACharacter>(Pawn))
			{
				if (UCharacterMovementComponent* MoveComp = Character->GetCharacterMovement())
				{
					MoveComp->StopMovementImmediately();
				}
			}
			
			if (!InstanceData.CachedCombatComponent->RequestPatternByPriority(InstanceData.TargetPlayer
				, RetrieveGameplayTags::Ability_Enemy_Attack))
			{
				return EStateTreeRunStatus::Failed;
			}

			return EStateTreeRunStatus::Running;
		}
	}
	else if (InstanceData.bStartAttack)
	{
		if (!InstanceData.CachedCombatComponent.IsValid())
		{
			return EStateTreeRunStatus::Failed;
		}

		const bool bPatternActive = InstanceData.CachedCombatComponent->IsPatternActive();
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

void FStateTreeTask_EnemyAttack::ExitState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime = 0.f;
	InstanceData.TimeInSoftAttackRange = 0.f;
	InstanceData.TimeSinceAttackRequested = 0.f;
	
	InstanceData.bStartAttack = false;
	InstanceData.bObservedPatternActive = false;
	
	InstanceData.LastMoveRequestLocation = FVector::ZeroVector;
	
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
		Enc->ReleaseAttackToken(InstanceData.TargetPlayer, Pawn);
	}
}
