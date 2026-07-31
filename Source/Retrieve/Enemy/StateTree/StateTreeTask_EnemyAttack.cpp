#include "Enemy/StateTree/StateTreeTask_EnemyAttack.h"

#include "StateTreeLinker.h"
#include "StateTreeExecutionContext.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/Enemy/EnemyCombatComponent.h"
#include "Enemy/EncirclementSubsystem.h"
#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Character/RetrieveEnemyCharacter.h"

namespace
{
	void SetChaseAnimationTag(APawn* Pawn, const bool bChasing)
	{
		if (!Pawn)
		{
			return;
		}

		UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Pawn);
		if (!ASC)
		{
			return;
		}

		const FGameplayTag ChaseTag = RetrieveGameplayTags::State_Enemy_Chase;
		if (bChasing)
		{
			if (!ASC->HasMatchingGameplayTag(ChaseTag))
			{
				ASC->AddLooseGameplayTag(ChaseTag);
			}
		}
		else if (ASC->HasMatchingGameplayTag(ChaseTag))
		{
			ASC->RemoveLooseGameplayTag(ChaseTag);
		}
	}

	bool ShouldUseForwardLocomotion(const APawn* Pawn)
	{
		const ARetrieveEnemyCharacter* EnemyCharacter = Cast<ARetrieveEnemyCharacter>(Pawn);
		return EnemyCharacter && EnemyCharacter->UsesForwardLocomotion();
	}

	bool FaceTargetForAttack(APawn* Pawn, AActor* Target, float DeltaTime, float AcceptanceAngle, float InterpSpeed)
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
			if (ARetrieveEnemyCharacter* EnemyCharacter = Cast<ARetrieveEnemyCharacter>(Pawn))
			{
				EnemyCharacter->StopGroundTurnAnimation();
			}
			return true;
		}

		if (ARetrieveEnemyCharacter* EnemyCharacter = Cast<ARetrieveEnemyCharacter>(Pawn))
		{
			EnemyCharacter->UpdateGroundTurnAnimation(SignedDeltaYaw);
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
	InstanceData.ApproachElapsedTime = 0.f;

	InstanceData.bStartAttack = false;
	InstanceData.bObservedPatternActive = false;
	InstanceData.bAttackTokenAcquired = false;
	InstanceData.bRotationSettingsCaptured = false;

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

	// Evaluator가 이미 bAttackApproachable(MaxActivationRange 기준)로 패턴 유효성을 검사했다.
	// 여기서 다시 검사하면 0.2초 갱신 간격 차이로 진입 직후 즉시 실패할 수 있어 제거한다.
	// 실제 공격 요청 직전(Tick)에서 최종적으로 다시 검사한다.

	UEncirclementSubsystem* EncircleSubsystem = Pawn->GetWorld()->GetSubsystem<UEncirclementSubsystem>();
	if (!EncircleSubsystem || !EncircleSubsystem->RequestAttackToken(InstanceData.TargetPlayer, Pawn))
	{
		return EStateTreeRunStatus::Failed;
	}
	InstanceData.bAttackTokenAcquired = true;

	if (UCharacterMovementComponent* MoveComp = Pawn->FindComponentByClass<UCharacterMovementComponent>())
	{
		InstanceData.bOriginalUseRVOAvoidance = MoveComp->bUseRVOAvoidance;

		const bool bForwardLocomotion = ShouldUseForwardLocomotion(Pawn);

		InstanceData.bOriginalOrientRotationToMovement = MoveComp->bOrientRotationToMovement;
		InstanceData.bOriginalUseControllerDesiredRotation = MoveComp->bUseControllerDesiredRotation;
		InstanceData.bOriginalUseControllerRotationYaw = Pawn->bUseControllerRotationYaw;
		InstanceData.bRotationSettingsCaptured = true;

		MoveComp->bUseRVOAvoidance = false;

		if (!bForwardLocomotion)
		{
			MoveComp->bOrientRotationToMovement = false;
			MoveComp->bUseControllerDesiredRotation = false;
			Pawn->bUseControllerRotationYaw = false;
		}
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

	if (!IsValid(InstanceData.TargetPlayer))
	{
		SetChaseAnimationTag(Pawn, false);
		return EStateTreeRunStatus::Failed;
	}

	if (!InstanceData.CachedCombatComponent.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!InstanceData.bStartAttack)
	{
		InstanceData.ApproachElapsedTime += DeltaTime;
		if (InstanceData.MaxApproachDuration > 0.f
			&& InstanceData.ApproachElapsedTime >= InstanceData.MaxApproachDuration)
		{
			return EStateTreeRunStatus::Failed;
		}

		if (InstanceData.SelectedPatternRowName.IsNone())
		{
			return EStateTreeRunStatus::Failed;
		}

		const UDataTable* PatternTable = InstanceData.CachedCombatComponent->GetPatternTable();
		const FMonsterPatternRow* CandidateRow = PatternTable
			                                         ? PatternTable->FindRow<FMonsterPatternRow>(
				                                         InstanceData.SelectedPatternRowName,
				                                         TEXT("StateTreeTask_EnemyAttack"))
			                                         : nullptr;

		if (!CandidateRow)
		{
			return EStateTreeRunStatus::Failed;
		}

		const float MaxRange = CandidateRow->MaxActivationRange;
		const float MinRange = CandidateRow->MinActivationRange;
		const float AttackStartRange = MaxRange + InstanceData.AttackStartRangeTolerance;

		constexpr float RangeHysteresis = 150.f;

		const float FarThreshold = InstanceData.bInAttackWindow
			                           ? AttackStartRange + RangeHysteresis
			                           : AttackStartRange;

		const float NearThreshold = InstanceData.bInAttackWindow
			                            ? FMath::Max(0.f, MinRange - RangeHysteresis)
			                            : MinRange;

		const FVector AttackMoveTarget = InstanceData.TargetPlayer->GetActorLocation();
		const float CurrentDistanceToTarget = FVector::Dist2D(Pawn->GetActorLocation(), AttackMoveTarget);

		// 먼 경우
		if (CurrentDistanceToTarget > FarThreshold)
		{
			InstanceData.bInAttackWindow = false;
			InstanceData.TimeInSoftAttackRange = 0.f;

			if (AAIController* AIC = Pawn->GetController<AAIController>())
			{
				const bool bCanMove = !InstanceData.CachedCombatComponent->IsMovementLockedByAttack();

				if (bCanMove)
				{
					SetChaseAnimationTag(Pawn, true);

					if (!ShouldUseForwardLocomotion(Pawn))
					{
						FaceTargetForAttack(
							Pawn, InstanceData.TargetPlayer, DeltaTime,
							InstanceData.FacingAcceptanceAngle, InstanceData.FacingInterpSpeed);
					}

					const float MoveDeltaSq = FVector::DistSquared2D(
						InstanceData.LastMoveRequestLocation, AttackMoveTarget);

					if (InstanceData.LastMoveRequestLocation.IsNearlyZero()
						|| MoveDeltaSq > FMath::Square(50.f))
					{
						AIC->MoveToLocation(AttackMoveTarget, InstanceData.MoveAcceptableRadius, true, true, true,
						                    false);
						InstanceData.LastMoveRequestLocation = AttackMoveTarget;
					}
				}
				else
				{
					SetChaseAnimationTag(Pawn, false);
				}
			}

			return EStateTreeRunStatus::Running;
		}

		// 가까운 경우
		if (NearThreshold > 0.f && CurrentDistanceToTarget < NearThreshold)
		{
			InstanceData.bInAttackWindow = false;
			InstanceData.TimeInSoftAttackRange = 0.f;

			if (AAIController* AIC = Pawn->GetController<AAIController>())
			{
				const bool bCanMove = !InstanceData.CachedCombatComponent->IsMovementLockedByAttack();

				if (bCanMove)
				{
					SetChaseAnimationTag(Pawn, true);

					FVector RetreatDir = Pawn->GetActorLocation() - AttackMoveTarget;
					RetreatDir.Z = 0.f;
					if (RetreatDir.IsNearlyZero())
					{
						RetreatDir = -Pawn->GetActorForwardVector();
					}
					RetreatDir.Normalize();

					const FVector RetreatTarget = AttackMoveTarget
						+ RetreatDir * (MinRange + InstanceData.AttackStartRangeTolerance + RangeHysteresis * 1.f);

					if (!ShouldUseForwardLocomotion(Pawn))
					{
						FaceTargetForAttack(
							Pawn, InstanceData.TargetPlayer, DeltaTime,
							InstanceData.FacingAcceptanceAngle, InstanceData.FacingInterpSpeed);
					}

					const float MoveDeltaSq = FVector::DistSquared2D(
						InstanceData.LastMoveRequestLocation, RetreatTarget);

					if (InstanceData.LastMoveRequestLocation.IsNearlyZero()
						|| MoveDeltaSq > FMath::Square(50.f))
					{
						AIC->MoveToLocation(RetreatTarget, InstanceData.MoveAcceptableRadius, true, true, true, false);
						InstanceData.LastMoveRequestLocation = RetreatTarget;
					}
				}
				else
				{
					SetChaseAnimationTag(Pawn, false);
				}
			}

			return EStateTreeRunStatus::Running;
		}


		// 사용
		SetChaseAnimationTag(Pawn, false);

		InstanceData.TimeInSoftAttackRange += DeltaTime;
		if (InstanceData.TimeInSoftAttackRange < InstanceData.AttackStartDelay)
		{
			return EStateTreeRunStatus::Running;
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

		if (!FaceTargetForAttack(
			Pawn, InstanceData.TargetPlayer, DeltaTime,
			InstanceData.FacingAcceptanceAngle, InstanceData.FacingInterpSpeed))
		{
			InstanceData.bStartAttack = false;
			return EStateTreeRunStatus::Running;
		}

		if (ARetrieveEnemyCharacter* EnemyCharacter = Cast<ARetrieveEnemyCharacter>(Pawn))
		{
			EnemyCharacter->StopGroundTurnAnimation();
		}

		if (!InstanceData.CachedCombatComponent->RequestSpecificPattern(
			InstanceData.TargetPlayer, InstanceData.SelectedPatternRowName, RetrieveGameplayTags::Ability_Enemy_Attack))
		{
			return EStateTreeRunStatus::Failed;
		}

		return EStateTreeRunStatus::Running;
	}
	else if (InstanceData.bStartAttack)
	{
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

	if (InstanceData.bStartAttack
		&& InstanceData.TimeSinceAttackRequested >= InstanceData.MaxAttackDuration)
	{
		return EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;
}

void FStateTreeTask_EnemyAttack::ExitState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const bool bStartedAttack = InstanceData.bStartAttack;
	const bool bHadAttackToken = InstanceData.bAttackTokenAcquired;

	InstanceData.ElapsedTime = 0.f;
	InstanceData.TimeInSoftAttackRange = 0.f;
	InstanceData.TimeSinceAttackRequested = 0.f;
	InstanceData.ApproachElapsedTime = 0.f;

	InstanceData.bStartAttack = false;
	InstanceData.bObservedPatternActive = false;
	InstanceData.bAttackTokenAcquired = false;

	InstanceData.LastMoveRequestLocation = FVector::ZeroVector;

	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	if (!Pawn)
	{
		InstanceData.bRotationSettingsCaptured = false;
		return;
	}

	if (UCharacterMovementComponent* MoveComp = Pawn->FindComponentByClass<UCharacterMovementComponent>())
	{
		if (InstanceData.bRotationSettingsCaptured)
		{
			MoveComp->bUseRVOAvoidance = InstanceData.bOriginalUseRVOAvoidance;
			MoveComp->bOrientRotationToMovement = InstanceData.bOriginalOrientRotationToMovement;
			MoveComp->bUseControllerDesiredRotation = InstanceData.bOriginalUseControllerDesiredRotation;
			Pawn->bUseControllerRotationYaw = InstanceData.bOriginalUseControllerRotationYaw;
		}
	}

	InstanceData.bRotationSettingsCaptured = false;

	if (bStartedAttack)
	{
		if (UEnemyCombatComponent* Combat = Pawn->FindComponentByClass<UEnemyCombatComponent>())
		{
			Combat->StopCurrentPattern();
		}
	}

	if (ARetrieveEnemyCharacter* EnemyCharacter = Cast<ARetrieveEnemyCharacter>(Pawn))
	{
		EnemyCharacter->StopGroundTurnAnimation();
	}

	SetChaseAnimationTag(Pawn, false);

	if (bHadAttackToken)
	{
		if (UEncirclementSubsystem* Enc = Pawn->GetWorld()->GetSubsystem<UEncirclementSubsystem>())
		{
			Enc->ReleaseAttackToken(InstanceData.TargetPlayer, Pawn);
		}
	}
}
