#include "Enemy/StateTree/StateTreeTask_EnemyAerialLanding.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Character/RetrieveEnemyCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"

namespace
{
constexpr float LandingGroundTraceUp = 250.f;
constexpr float LandingGroundTraceDown = 6000.f;
constexpr float LandingGroundSnapTolerance = 8.f;

UAnimInstance* GetEnemyAnimInstance(const ARetrieveEnemyCharacter* Enemy)
{
	if (!IsValid(Enemy))
	{
		return nullptr;
	}

	const USkeletalMeshComponent* Mesh = Enemy->GetMesh();
	return Mesh ? Mesh->GetAnimInstance() : nullptr;
}

UAnimSequenceBase* ResolveLandingAnimation(const TSoftObjectPtr<UAnimSequenceBase>& Animation)
{
	return Animation.IsNull() ? nullptr : Animation.LoadSynchronous();
}

void PlayLandingAnimation(
	const ARetrieveEnemyCharacter* Enemy,
	const TSoftObjectPtr<UAnimSequenceBase>& Animation,
	const float PlayRate)
{
	UAnimInstance* AnimInstance = GetEnemyAnimInstance(Enemy);
	UAnimSequenceBase* LoadedAnimation = ResolveLandingAnimation(Animation);
	if (!AnimInstance || !LoadedAnimation)
	{
		return;
	}

	AnimInstance->PlaySlotAnimationAsDynamicMontage(
		LoadedAnimation,
		TEXT("DefaultSlot"),
		0.15f,
		0.15f,
		PlayRate,
		1);
}

bool TraceWorldGroundAt(const ARetrieveEnemyCharacter* Enemy, const FVector& XYLocation, FHitResult& OutHit)
{
	if (!IsValid(Enemy) || !Enemy->GetWorld())
	{
		return false;
	}

	const FVector TraceStart(XYLocation.X, XYLocation.Y, XYLocation.Z + LandingGroundTraceUp);
	const FVector TraceEnd(XYLocation.X, XYLocation.Y, XYLocation.Z - LandingGroundTraceDown);
	FCollisionQueryParams Params(FName(TEXT("EnemyAerialLandingGroundTrace")), false, Enemy);
	return Enemy->GetWorld()->LineTraceSingleByChannel(OutHit, TraceStart, TraceEnd, ECC_WorldStatic, Params);
}

bool IsEnemyOnWorldGround(const ARetrieveEnemyCharacter* Enemy)
{
	if (!IsValid(Enemy) || !Enemy->GetCapsuleComponent())
	{
		return false;
	}

	const float HalfHeight = Enemy->GetCapsuleComponent()
		? Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
		: 0.f;

	FHitResult Hit;
	if (!TraceWorldGroundAt(Enemy, Enemy->GetActorLocation(), Hit))
	{
		return false;
	}

	const float CapsuleBottomZ = Enemy->GetActorLocation().Z - HalfHeight;
	return FMath::Abs(CapsuleBottomZ - Hit.ImpactPoint.Z) <= LandingGroundSnapTolerance;
}

bool TryPlaceEnemyAtGroundLocation(ARetrieveEnemyCharacter* Enemy, const FVector& ProbeLocation)
{
	if (!IsValid(Enemy) || !Enemy->GetWorld() || !Enemy->GetCapsuleComponent())
	{
		return false;
	}

	FHitResult Hit;
	if (!TraceWorldGroundAt(Enemy, ProbeLocation, Hit))
	{
		return false;
	}

	const float HalfHeight = Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FVector TargetLocation = Hit.ImpactPoint + FVector(0.f, 0.f, HalfHeight);
	const FRotator TargetRotation = Enemy->GetActorRotation();

	if (!Enemy->GetWorld()->FindTeleportSpot(Enemy, TargetLocation, TargetRotation))
	{
		return false;
	}

	Enemy->SetActorLocation(TargetLocation, false, nullptr, ETeleportType::TeleportPhysics);
	return true;
}

bool SnapEnemyToGround(ARetrieveEnemyCharacter* Enemy)
{
	if (!IsValid(Enemy) || !Enemy->GetCapsuleComponent())
	{
		return false;
	}

	const FVector ActorLocation = Enemy->GetActorLocation();
	const float CapsuleRadius = Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius();
	const float Step = FMath::Max(160.f, CapsuleRadius * 2.5f);

	if (TryPlaceEnemyAtGroundLocation(Enemy, ActorLocation))
	{
		return true;
	}

	static const FVector2D Directions[] = {
		FVector2D(1.f, 0.f),
		FVector2D(-1.f, 0.f),
		FVector2D(0.f, 1.f),
		FVector2D(0.f, -1.f),
		FVector2D(0.707f, 0.707f),
		FVector2D(0.707f, -0.707f),
		FVector2D(-0.707f, 0.707f),
		FVector2D(-0.707f, -0.707f),
	};

	for (int32 Ring = 1; Ring <= 3; ++Ring)
	{
		for (const FVector2D& Direction : Directions)
		{
			const FVector CandidateLocation = ActorLocation + FVector(Direction.X * Step * Ring, Direction.Y * Step * Ring, 0.f);
			if (TryPlaceEnemyAtGroundLocation(Enemy, CandidateLocation))
			{
				return true;
			}
		}
	}

	return false;
}
}

bool FStateTreeTask_EnemyAerialLanding::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(PawnHandle);
	return true;
}

EStateTreeRunStatus FStateTreeTask_EnemyAerialLanding::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& ID = Context.GetInstanceData(*this);
	ID.ElapsedTime = 0.f;
	ID.CachedEnemy = nullptr;

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

	PlayLandingAnimation(ID.CachedEnemy.Get(), ID.LandingAnimation, ID.MontagePlayRate);
	ID.CachedEnemy->SetAerialMode(false);
	if (UCharacterMovementComponent* MoveComp = ID.CachedEnemy->GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Falling);
		MoveComp->Velocity.Z = FMath::Min(MoveComp->Velocity.Z, -10.f);
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_EnemyAerialLanding::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	FInstanceDataType& ID = Context.GetInstanceData(*this);
	ID.ElapsedTime += DeltaTime;

	if (!ID.CachedEnemy.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	if (const UCharacterMovementComponent* MoveComp = ID.CachedEnemy->GetCharacterMovement())
	{
		if (MoveComp->MovementMode == MOVE_Walking || MoveComp->MovementMode == MOVE_NavWalking)
		{
			if (IsEnemyOnWorldGround(ID.CachedEnemy.Get()))
			{
				return EStateTreeRunStatus::Succeeded;
			}
		}
	}

	if (ID.ElapsedTime >= ID.LandingTimeout)
	{
		SnapEnemyToGround(ID.CachedEnemy.Get());
		ID.CachedEnemy->SetAerialMode(false);
		if (UCharacterMovementComponent* MoveComp = ID.CachedEnemy->GetCharacterMovement())
		{
			MoveComp->SetMovementMode(MOVE_Walking);
			MoveComp->Velocity = FVector::ZeroVector;
		}
		return EStateTreeRunStatus::Succeeded;
	}

	return EStateTreeRunStatus::Running;
}

void FStateTreeTask_EnemyAerialLanding::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& ID = Context.GetInstanceData(*this);

	if (ID.CachedEnemy.IsValid())
	{
		const UCharacterMovementComponent* MoveComp = ID.CachedEnemy->GetCharacterMovement();
		if (!MoveComp || MoveComp->MovementMode != MOVE_Falling)
		{
			ID.CachedEnemy->SetAerialMode(false);
		}
	}

	ID.ElapsedTime = 0.f;
	ID.CachedEnemy = nullptr;
}
