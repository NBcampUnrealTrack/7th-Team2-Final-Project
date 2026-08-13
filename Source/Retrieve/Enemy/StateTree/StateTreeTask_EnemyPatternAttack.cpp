#include "Enemy/StateTree/StateTreeTask_EnemyPatternAttack.h"

#include "StateTreeLinker.h"
#include "StateTreeExecutionContext.h"
#include "Components/Enemy/EnemyCombatComponent.h"
#include "Enemy/EncirclementSubsystem.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Logging/RetrieveLogChannels.h"
#include "Character/RetrieveEnemyCharacter.h"

namespace
{
	bool FaceTargetForPatternAttack(
		APawn* Pawn,
		AActor* Target,
		float DeltaTime,
		float AcceptanceAngle,
		float InterpSpeed,
		bool bUseGroundTurnAnimation)
	{
		if (!Pawn || !Target)
		{
			return true;
		}

		FVector Direction = Target->GetActorLocation() - Pawn->GetActorLocation();
		Direction.Z = 0.f;
		if (Direction.IsNearlyZero())
		{
			return true;
		}

		const FRotator CurrentRotation = Pawn->GetActorRotation();
		const FRotator TargetRotation = Direction.Rotation();
		const float SignedDeltaYaw = FMath::FindDeltaAngleDegrees(CurrentRotation.Yaw, TargetRotation.Yaw);
		const float DeltaYaw = FMath::Abs(SignedDeltaYaw);
		if (DeltaYaw <= AcceptanceAngle)
		{
			if (bUseGroundTurnAnimation)
			{
				if (ARetrieveEnemyCharacter* EnemyCharacter = Cast<ARetrieveEnemyCharacter>(Pawn))
				{
					EnemyCharacter->StopGroundTurnAnimation();
				}
			}
			return true;
		}

		if (bUseGroundTurnAnimation)
		{
			if (ARetrieveEnemyCharacter* EnemyCharacter = Cast<ARetrieveEnemyCharacter>(Pawn))
			{
				EnemyCharacter->UpdateGroundTurnAnimation(SignedDeltaYaw);
			}
		}

		const FRotator NewRotation = FMath::RInterpTo(
			CurrentRotation,
			FRotator(CurrentRotation.Pitch, TargetRotation.Yaw, CurrentRotation.Roll),
			DeltaTime,
			InterpSpeed);
		Pawn->SetActorRotation(NewRotation);
		return false;
	}
}

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

	UEncirclementSubsystem* EncircleSubsystem = Pawn->GetWorld()->GetSubsystem<UEncirclementSubsystem>();
	const bool bTokenRequested =
	EncircleSubsystem && EncircleSubsystem->RequestAttackToken(InstanceData.TargetPlayer, Pawn);

	if (!bTokenRequested)
	{
		UE_LOG(LogRetrieveCombat, Verbose,
			TEXT("[EnemyPatternAttack] Special pattern continues without attack token. Pawn=%s Target=%s"),
			*GetNameSafe(Pawn),
			*GetNameSafe(InstanceData.TargetPlayer));
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

		// 재진입 시 이미 특수 패턴이 진행 중이면, 새로 요청하지 말고 그 패턴이 끝날 때까지 대기
		if (InstanceData.CachedCombatComponent->IsPatternActive())
		{
			InstanceData.bStartAttack = true;
			InstanceData.bObservedPatternActive = true;
			InstanceData.TimeSinceAttackRequested = 0.f;
			return EStateTreeRunStatus::Running;
		}
	
		// 회전과 턴 애니메이션은 갱신하지만, FacingAcceptanceAngle(기본 8°)로 특수공격 발동을 제한하지는 않는다.
		// RequestPatternByPriority가 발동 직전에 FaceTarget을 수행하므로,
		// 여기서 각도 게이트를 적용하면 전진 로코모션과 회전이 충돌해 특수공격이 계속 발동하지 못할 수 있다.
		FaceTargetForPatternAttack(
			Pawn,
			InstanceData.TargetPlayer,
			DeltaTime,
			InstanceData.FacingAcceptanceAngle,
			InstanceData.FacingInterpSpeed,
			InstanceData.bUseGroundTurnAnimation);

		const bool bRequested =
			InstanceData.CachedCombatComponent->RequestPatternByPriority(InstanceData.TargetPlayer, RetrieveGameplayTags::Ability_Enemy_SpecialAttack);

		if (!bRequested)
		{
			InstanceData.CachedCombatComponent->SuppressSpecialAttackEvaluation(
				InstanceData.SpecialFailureRecoveryLockDuration);
			return EStateTreeRunStatus::Succeeded;
		}

		InstanceData.bStartAttack = true;
		InstanceData.bObservedPatternActive = true;
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
			InstanceData.CachedCombatComponent->SuppressSpecialAttackEvaluation(
				InstanceData.SpecialFailureRecoveryLockDuration);
			return EStateTreeRunStatus::Succeeded;
		}
	}

	if (InstanceData.ElapsedTime >= InstanceData.MaxAttackDuration)
	{
		if (InstanceData.CachedCombatComponent.IsValid())
		{
			InstanceData.CachedCombatComponent->SuppressSpecialAttackEvaluation(
				InstanceData.SpecialFailureRecoveryLockDuration);
		}
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

void FStateTreeTask_EnemyPatternAttack::ExitState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	const bool bObservedPatternActive = InstanceData.bObservedPatternActive;

	InstanceData.ElapsedTime = 0.f;
	InstanceData.TimeSinceAttackRequested = 0.f;

	InstanceData.bStartAttack = false;
	InstanceData.bObservedPatternActive = false;

	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	if (!Pawn)
	{
		return;
	}

	UEnemyCombatComponent* Combat = Pawn->FindComponentByClass<UEnemyCombatComponent>();

	// 진행 중인 패턴은 유지하고, 요청 후 발동하지 못한 패턴만 정리한다.
	if (!bObservedPatternActive && Combat && !Combat->IsPatternActive())
	{
		Combat->StopCurrentPattern();
	}

	if (InstanceData.bUseGroundTurnAnimation)
	{
		if (ARetrieveEnemyCharacter* EnemyCharacter = Cast<ARetrieveEnemyCharacter>(Pawn))
		{
			EnemyCharacter->StopGroundTurnAnimation();
		}
	}

	if (UEncirclementSubsystem* Enc = Pawn->GetWorld()->GetSubsystem<UEncirclementSubsystem>())
	{
		Enc->ReleaseAttackToken(InstanceData.TargetPlayer, Pawn);
	}
}
