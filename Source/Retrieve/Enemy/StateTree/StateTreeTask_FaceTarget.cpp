#include "Enemy/StateTree/StateTreeTask_FaceTarget.h"

#include "StateTreeLinker.h"
#include "StateTreeExecutionContext.h"
#include "Character/RetrieveEnemyCharacter.h"

namespace
{
	// StateTreeTask_EnemyAttack.cpp의 FaceTargetForAttack과 동일한 방식(RInterpTo 기반 Yaw 보간).
	void FaceTargetSlowly(APawn* Pawn, AActor* Target, float DeltaTime, float AcceptanceAngle, float InterpSpeed)
	{
		if (!Pawn || !Target)
		{
			return;
		}

		FVector Direction = Target->GetActorLocation() - Pawn->GetActorLocation();
		Direction.Z = 0.f;
		if (Direction.IsNearlyZero())
		{
			return;
		}

		const FRotator CurrentRotation = Pawn->GetActorRotation();
		const FRotator TargetRotation = Direction.Rotation();
		const float SignedDeltaYaw = FMath::FindDeltaAngleDegrees(CurrentRotation.Yaw, TargetRotation.Yaw);
		if (FMath::Abs(SignedDeltaYaw) <= AcceptanceAngle)
		{
			return;
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
	}
}

bool FStateTreeTask_FaceTarget::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(PawnHandle);
	return true;
}

EStateTreeRunStatus FStateTreeTask_FaceTarget::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);

	FaceTargetSlowly(Pawn, InstanceData.TargetPlayer, DeltaTime, InstanceData.FacingAcceptanceAngle, InstanceData.FacingInterpSpeed);

	// State 이탈은 StateTree 전이 조건(Gauge/TargetLost)이 담당하므로 계속 Running.
	return EStateTreeRunStatus::Running;
}
