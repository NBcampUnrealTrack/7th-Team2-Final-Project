#include "Enemy/StateTree/StateTreeTask_EnemyAerialPhase.h"

#include "StateTreeLinker.h"
#include "StateTreeExecutionContext.h"
#include "Character/RetrieveEnemyCharacter.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/Enemy/EnemyCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameplayTags/RetrieveGameplayTags.h"

namespace
{
UAnimInstance* GetEnemyAnimInstance(const ARetrieveEnemyCharacter* Enemy)
{
	if (!IsValid(Enemy))
	{
		return nullptr;
	}

	const USkeletalMeshComponent* Mesh = Enemy->GetMesh();
	return Mesh ? Mesh->GetAnimInstance() : nullptr;
}

UAnimSequenceBase* ResolveAerialAnimation(const TSoftObjectPtr<UAnimSequenceBase>& Animation)
{
	return Animation.IsNull() ? nullptr : Animation.LoadSynchronous();
}

UAnimMontage* PlayAerialAnimation(
	const ARetrieveEnemyCharacter* Enemy,
	const TSoftObjectPtr<UAnimSequenceBase>& Animation,
	const float PlayRate,
	const int32 LoopCount)
{
	UAnimInstance* AnimInstance = GetEnemyAnimInstance(Enemy);
	UAnimSequenceBase* LoadedAnimation = ResolveAerialAnimation(Animation);
	if (!AnimInstance || !LoadedAnimation)
	{
		return nullptr;
	}

	return AnimInstance->PlaySlotAnimationAsDynamicMontage(
		LoadedAnimation,
		TEXT("DefaultSlot"),
		0.15f,
		0.15f,
		PlayRate,
		LoopCount);
}

bool IsAerialAnimationPlaying(const ARetrieveEnemyCharacter* Enemy, const TSoftObjectPtr<UAnimSequenceBase>& Animation)
{
	UAnimInstance* AnimInstance = GetEnemyAnimInstance(Enemy);
	UAnimSequenceBase* LoadedAnimation = ResolveAerialAnimation(Animation);
	return AnimInstance && LoadedAnimation && AnimInstance->IsPlayingSlotAnimation(LoadedAnimation, TEXT("DefaultSlot"));
}
}

bool FStateTreeTask_EnemyAerialPhase::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(PawnHandle);
	return true;
}

EStateTreeRunStatus FStateTreeTask_EnemyAerialPhase::EnterState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& ID = Context.GetInstanceData(*this);

	ID.bReachedHoverHeight = false;
	ID.ElapsedHoverTime    = 0.f;
	ID.TotalElapsedTime    = 0.f;
	ID.bPlayedFlightMontage = false;
	ID.bPlayedHoverMontage = false;
	ID.bRequestedSpecialPattern = false;
	ID.bLandingStarted = false;
	ID.LandingElapsedTime = 0.f;
	ID.TimeSinceSpecialAttackRequest = ID.SpecialAttackRetryInterval;

	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (!IsValid(ID.TargetPlayer))
	{
		return EStateTreeRunStatus::Failed;
	}

	ID.CachedEnemy = Cast<ARetrieveEnemyCharacter>(Pawn);
	if (!ID.CachedEnemy.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}
	ID.CachedCombatComponent = Pawn->FindComponentByClass<UEnemyCombatComponent>();

	// 현재 Z를 지면 기준으로 저장 (상승 후 돌아올 높이 계산에 사용)
	ID.GroundZ = Pawn->GetActorLocation().Z;

	ID.CachedEnemy->SetAerialMode(true);
	PlayAerialAnimation(ID.CachedEnemy.Get(), ID.TakeOffAnimation, ID.MontagePlayRate, 1);

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_EnemyAerialPhase::Tick(
	FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& ID = Context.GetInstanceData(*this);
	ID.TotalElapsedTime += DeltaTime;

	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	if (!Pawn || !ID.CachedEnemy.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	const FVector CurrentLoc = Pawn->GetActorLocation();

	if (ID.bLandingStarted)
	{
		ID.LandingElapsedTime += DeltaTime;

		if (const UCharacterMovementComponent* MoveComp = ID.CachedEnemy->GetCharacterMovement())
		{
			if (MoveComp->MovementMode == MOVE_Walking || MoveComp->MovementMode == MOVE_NavWalking)
			{
				return EStateTreeRunStatus::Succeeded;
			}
		}

		if (ID.LandingElapsedTime >= ID.LandingTimeout)
		{
			return EStateTreeRunStatus::Succeeded;
		}

		return EStateTreeRunStatus::Running;
	}

	// 목표 호버 위치: 플레이어 XY + (지면Z + HoverHeight)
	FVector HoverTarget = CurrentLoc;
	if (IsValid(ID.TargetPlayer))
	{
		const FVector PlayerLoc = ID.TargetPlayer->GetActorLocation();
		HoverTarget = FVector(PlayerLoc.X, PlayerLoc.Y, ID.GroundZ + ID.HoverHeight);

		// 플레이어를 향해 회전 (XY 평면만)
		const FVector FaceDir = (PlayerLoc - CurrentLoc).GetSafeNormal2D();
		if (!FaceDir.IsNearlyZero())
		{
			Pawn->SetActorRotation(FaceDir.Rotation());
		}
	}
	else
	{
		HoverTarget = FVector(CurrentLoc.X, CurrentLoc.Y, ID.GroundZ + ID.HoverHeight);
	}

	// 호버 목표로 이동 입력
	const FVector Delta = HoverTarget - CurrentLoc;
	if (Delta.Size() > ID.PositionTolerance)
	{
		Pawn->AddMovementInput(Delta.GetSafeNormal(), 1.f);
		if (!ID.bPlayedFlightMontage && !IsAerialAnimationPlaying(ID.CachedEnemy.Get(), ID.TakeOffAnimation))
		{
			PlayAerialAnimation(ID.CachedEnemy.Get(), ID.FlightAnimation, ID.MontagePlayRate, 999);
			ID.bPlayedFlightMontage = true;
		}
	}
	else
	{
		// 목표 위치 도달 → 호버 시간 카운트 시작
		ID.bReachedHoverHeight = true;
		if (!ID.bPlayedHoverMontage)
		{
			PlayAerialAnimation(ID.CachedEnemy.Get(), ID.HoverAnimation, ID.MontagePlayRate, 999);
			ID.bPlayedHoverMontage = true;
		}
		if (!ID.bRequestedSpecialPattern && ID.CachedCombatComponent.IsValid())
		{
			ID.TimeSinceSpecialAttackRequest += DeltaTime;
			if (ID.TimeSinceSpecialAttackRequest >= ID.SpecialAttackRetryInterval)
			{
				ID.TimeSinceSpecialAttackRequest = 0.f;
				ID.bRequestedSpecialPattern = ID.CachedCombatComponent->RequestPatternByPriority(
					ID.TargetPlayer,
					RetrieveGameplayTags::Ability_Enemy_SpecialAttack);
			}
		}
	}

	// 호버 도달 후 경과 시간 누적
	if (ID.bReachedHoverHeight)
	{
		ID.ElapsedHoverTime += DeltaTime;
		if (ID.ElapsedHoverTime >= ID.HoverDuration)
		{
			PlayAerialAnimation(ID.CachedEnemy.Get(), ID.LandingAnimation, ID.MontagePlayRate, 1);
			ID.CachedEnemy->SetAerialMode(false);
			ID.bLandingStarted = true;
			ID.LandingElapsedTime = 0.f;
			return EStateTreeRunStatus::Running;
		}
	}

	// 안전 타임아웃: 상승 포함 전체 시간이 HoverDuration + 10초 초과 시 강제 종료
	if (ID.TotalElapsedTime >= ID.HoverDuration + 10.f)
	{
		return EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;
}

void FStateTreeTask_EnemyAerialPhase::ExitState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& ID = Context.GetInstanceData(*this);

	if (ID.CachedEnemy.IsValid() && !ID.bLandingStarted)
	{
		// MOVE_Falling으로 복귀 → 착지 시 자동으로 MOVE_Walking 전환
		PlayAerialAnimation(ID.CachedEnemy.Get(), ID.LandingAnimation, ID.MontagePlayRate, 1);
		ID.CachedEnemy->SetAerialMode(false);
	}

	// 특수 공격 성공/실패 여부와 무관하게 쿨타임 재시작 → 지상 Chase/Attack 복귀 보장
	if (ID.CachedCombatComponent.IsValid())
	{
		ID.CachedCombatComponent->StartSpecialAttackRetryCooldown();
	}

	ID.bReachedHoverHeight = false;
	ID.ElapsedHoverTime    = 0.f;
	ID.TotalElapsedTime    = 0.f;
	ID.bPlayedFlightMontage = false;
	ID.bPlayedHoverMontage = false;
	ID.bRequestedSpecialPattern = false;
	ID.bLandingStarted = false;
	ID.LandingElapsedTime = 0.f;
	ID.TimeSinceSpecialAttackRequest = 0.f;
	ID.CachedEnemy         = nullptr;
	ID.CachedCombatComponent = nullptr;
}
