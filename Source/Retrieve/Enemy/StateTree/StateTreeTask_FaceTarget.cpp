#include "Enemy/StateTree/StateTreeTask_FaceTarget.h"

#include "StateTreeLinker.h"
#include "StateTreeExecutionContext.h"
#include "Character/RetrieveEnemyCharacter.h"

namespace
{
	// 공격 회전과 동일하게 허용 각도 밖에서만 Yaw를 보간하고 턴 애니메이션을 갱신한다.
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

	// 종료 조건은 StateTree 전이가 판단하므로 Task는 Running을 유지한다.
	return EStateTreeRunStatus::Running;
}
