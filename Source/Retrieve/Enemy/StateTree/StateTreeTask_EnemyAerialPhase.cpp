#include "Enemy/StateTree/StateTreeTask_EnemyAerialPhase.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Character/RetrieveEnemyCharacter.h"
#include "Components/Enemy/EnemyCombatComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"

namespace
{
UAnimInstance* GetAerialPhaseEnemyAnimInstance(const ARetrieveEnemyCharacter* Enemy)
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
	UAnimInstance* AnimInstance = GetAerialPhaseEnemyAnimInstance(Enemy);
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
	UAnimInstance* AnimInstance = GetAerialPhaseEnemyAnimInstance(Enemy);
	UAnimSequenceBase* LoadedAnimation = ResolveAerialAnimation(Animation);
	return AnimInstance && LoadedAnimation && AnimInstance->IsPlayingSlotAnimation(LoadedAnimation, TEXT("DefaultSlot"));
}

float ResolveGroundZ(const APawn& Pawn, const float FallbackZ)
{
	UWorld* World = Pawn.GetWorld();
	if (!World)
	{
		return FallbackZ;
	}

	const FVector ActorLocation = Pawn.GetActorLocation();
	const FVector TraceStart = ActorLocation + FVector(0.f, 0.f, 150.f);
	const FVector TraceEnd = ActorLocation - FVector(0.f, 0.f, 5000.f);

	FHitResult Hit;
	FCollisionQueryParams Params(FName(TEXT("EnemyAerialPhaseGroundTrace")), false, &Pawn);
	if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, Params))
	{
		return Hit.ImpactPoint.Z;
	}

	return FallbackZ;
}
}

bool FStateTreeTask_EnemyAerialPhase::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(PawnHandle);
	return true;
}

EStateTreeRunStatus FStateTreeTask_EnemyAerialPhase::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& ID = Context.GetInstanceData(*this);
	ID.ElapsedTime = 0.f;
	ID.HoverLocation = FVector::ZeroVector;
	ID.bPlayedFlightMontage = false;
	ID.bPlayedHoverMontage = false;
	ID.CachedEnemy = nullptr;

	if (!ID.bHasAerialPhase || !IsValid(ID.TargetPlayer))
	{
		return EStateTreeRunStatus::Failed;
	}

	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	ID.CachedEnemy = Cast<ARetrieveEnemyCharacter>(Pawn);
	if (!ID.CachedEnemy.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	ID.GroundZ = ResolveGroundZ(*Pawn, Pawn->GetActorLocation().Z - ID.HoverHeight);
	const FVector TargetLocation = ID.TargetPlayer->GetActorLocation();
	ID.HoverLocation = FVector(TargetLocation.X, TargetLocation.Y, ID.GroundZ + ID.HoverHeight);

	if (UEnemyCombatComponent* CombatComponent = Pawn->FindComponentByClass<UEnemyCombatComponent>())
	{
		CombatComponent->SuppressSpecialAttackEvaluation(ID.ReentryLockDuration);
	}

	ID.CachedEnemy->SetAerialMode(true);
	PlayAerialAnimation(ID.CachedEnemy.Get(), ID.TakeOffAnimation, ID.MontagePlayRate, 1);

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_EnemyAerialPhase::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& ID = Context.GetInstanceData(*this);
	ID.ElapsedTime += DeltaTime;

	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	if (!Pawn || !ID.CachedEnemy.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	const FVector CurrentLoc = Pawn->GetActorLocation();
	if (IsValid(ID.TargetPlayer))
	{
		const FVector PlayerLoc = ID.TargetPlayer->GetActorLocation();
		const FVector FaceDir = (PlayerLoc - CurrentLoc).GetSafeNormal2D();
		if (!FaceDir.IsNearlyZero())
		{
			Pawn->SetActorRotation(FaceDir.Rotation());
		}
	}

	if (ID.HoverLocation.IsNearlyZero())
	{
		ID.HoverLocation = FVector(CurrentLoc.X, CurrentLoc.Y, ID.GroundZ + ID.HoverHeight);
	}

	const FVector Delta = ID.HoverLocation - CurrentLoc;
	if (Delta.Size() <= ID.PositionTolerance && !ID.bPlayedHoverMontage)
	{
		PlayAerialAnimation(ID.CachedEnemy.Get(), ID.HoverAnimation, ID.MontagePlayRate, 999);
		ID.bPlayedHoverMontage = true;
	}

	if (ID.ElapsedTime >= ID.AttackDelay)
	{
		if (UPawnMovementComponent* MoveComp = Pawn->GetMovementComponent())
		{
			MoveComp->StopMovementImmediately();
		}
		return EStateTreeRunStatus::Succeeded;
	}

	if (Delta.Size() > ID.PositionTolerance)
	{
		Pawn->AddMovementInput(Delta.GetSafeNormal(), 1.f);
	}
	if (!ID.bPlayedFlightMontage && !IsAerialAnimationPlaying(ID.CachedEnemy.Get(), ID.TakeOffAnimation))
	{
		PlayAerialAnimation(ID.CachedEnemy.Get(), ID.FlightAnimation, ID.MontagePlayRate, 999);
		ID.bPlayedFlightMontage = true;
	}

	if (ID.ElapsedTime >= ID.MaxPhaseDuration)
	{
		if (UPawnMovementComponent* MoveComp = Pawn->GetMovementComponent())
		{
			MoveComp->StopMovementImmediately();
		}
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

void FStateTreeTask_EnemyAerialPhase::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& ID = Context.GetInstanceData(*this);
	ID.ElapsedTime = 0.f;
	ID.HoverLocation = FVector::ZeroVector;
	ID.bPlayedFlightMontage = false;
	ID.bPlayedHoverMontage = false;
	ID.CachedEnemy = nullptr;
}
