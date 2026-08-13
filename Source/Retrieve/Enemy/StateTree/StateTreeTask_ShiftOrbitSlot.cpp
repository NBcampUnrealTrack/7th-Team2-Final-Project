#include "Enemy/StateTree/StateTreeTask_ShiftOrbitSlot.h"

#include "StateTreeLinker.h"
#include "StateTreeExecutionContext.h"
#include "Components/Enemy/EnemyCombatComponent.h"
#include "Enemy/EncirclementSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"
#include "Character/RetrieveEnemyCharacter.h"

namespace
{
	bool ShouldUseShiftOrbitForwardLocomotion(const APawn* Pawn)
	{
		const ARetrieveEnemyCharacter* EnemyCharacter = Cast<ARetrieveEnemyCharacter>(Pawn);
		return EnemyCharacter && EnemyCharacter->UsesForwardLocomotion();
	}

	bool ShouldFaceTargetDuringShiftOrbit(const APawn* Pawn, const AActor* Target)
	{
		const ARetrieveEnemyCharacter* EnemyCharacter = Cast<ARetrieveEnemyCharacter>(Pawn);
		return EnemyCharacter
			&& EnemyCharacter->ShouldFaceTargetDuringShiftOrbit()
			&& IsValid(Target);
	}

	void ApplyShiftOrbitFacing(
		APawn* Pawn,
		UCharacterMovementComponent* CharacterMovement,
		AActor* Target)
	{
		if (!Pawn || !CharacterMovement)
		{
			return;
		}

		const bool bFaceTarget = ShouldFaceTargetDuringShiftOrbit(Pawn, Target);
		const bool bUseForwardLocomotion = ShouldUseShiftOrbitForwardLocomotion(Pawn);
		// bUseControllerRotationYaw는 매 프레임 즉시 스냅(보간 없음)이라 bUseControllerDesiredRotation의
		// RotationRate 보간과 동시에 켜면 스냅이 그대로 이겨버린다. 항상 false로 두고 보간만 사용한다.
		Pawn->bUseControllerRotationYaw = false;
		CharacterMovement->bOrientRotationToMovement = !bFaceTarget && bUseForwardLocomotion;
		CharacterMovement->bUseControllerDesiredRotation = bFaceTarget || !bUseForwardLocomotion;

		if (AAIController* AIC = Pawn->GetController<AAIController>())
		{
			if (bFaceTarget)
			{
				AIC->SetFocus(Target, EAIFocusPriority::Gameplay);
			}
			else
			{
				AIC->ClearFocus(EAIFocusPriority::Gameplay);
			}
		}
	}
}

bool FStateTreeTask_ShiftOrbitSlot::Link(FStateTreeLinker& Linker)
{
	Linker.LinkExternalData(PawnHandle);
	return true;
}

EStateTreeRunStatus FStateTreeTask_ShiftOrbitSlot::EnterState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	// 도착 대기 사용 시 이동 중 시간이 누적되지 않도록 0에서 시작한다.
	// 미사용 시 첫 Tick부터 슬롯 이동을 평가하도록 StrafeInterval에서 시작한다.
	InstanceData.ElapsedTime = InstanceData.bWaitForArrivalBeforeShift
		? 0.f
		: InstanceData.StrafeInterval;
	
	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	UEnemyCombatComponent* Combat = Pawn->FindComponentByClass<UEnemyCombatComponent>();
	if (!Combat)
	{
		return EStateTreeRunStatus::Failed;
	}
	
	UCharacterMovementComponent* CharacterMovement = Pawn->FindComponentByClass<UCharacterMovementComponent>();
	if (!IsValid(CharacterMovement))
	{
		return EStateTreeRunStatus::Failed;
	}
	
	InstanceData.bOriginalOrient = CharacterMovement->bOrientRotationToMovement;
	InstanceData.bOriginalControllerRot = CharacterMovement->bUseControllerDesiredRotation;
	InstanceData.bOriginalUseControllerRotationYaw = Pawn->bUseControllerRotationYaw;

	ApplyShiftOrbitFacing(Pawn, CharacterMovement, InstanceData.TargetActor);
	
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FStateTreeTask_ShiftOrbitSlot::Tick(
	FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// 도착 대기 사용 시 점유 수와 슬롯 도착 여부를 먼저 검사한다.
	// 슬롯이 없으면 즉시 배정을 요청하고 같은 Tick의 이동은 생략한다.
	// 미도착 중에는 ElapsedTime을 0으로 유지해 StrafeInterval이 누적되지 않게 한다.
	if (InstanceData.bWaitForArrivalBeforeShift)
	{
		if (APawn* GatePawn = Context.GetExternalDataPtr(PawnHandle))
		{
			UEncirclementSubsystem* GateEncSub = IsValid(InstanceData.TargetActor)
				? GatePawn->GetWorld()->GetSubsystem<UEncirclementSubsystem>()
				: nullptr;
			if (GateEncSub)
			{
				if (GateEncSub->GetCommittedCount(InstanceData.TargetActor)
					< InstanceData.MinOccupantsToCircle)
				{
					InstanceData.ElapsedTime = 0.f;
					return EStateTreeRunStatus::Running;
				}

				const int32 GateSlot = GateEncSub->GetCurrentSlot(InstanceData.TargetActor, GatePawn);
				if (GateSlot == INDEX_NONE)
				{
					// Evaluator의 선행 배정에 의존하지 않도록 현재 슬롯이 없으면 여기서 확보한다.
					const int32 RequestedSlot = GateEncSub->RequestSlot(InstanceData.TargetActor, GatePawn);
					if (RequestedSlot == INDEX_NONE)
					{
						UE_LOG(LogStateTree, Warning, TEXT("[%s] CurrentSlot not found and slot request failed"), *GatePawn->GetName());
					}
					
					// ChaseLocation 갱신 또는 슬롯 배정 재시도를 다음 Tick으로 미루고,
					// 같은 Tick의 슬롯 이동은 생략한다.
					InstanceData.ElapsedTime = 0.f;
					return EStateTreeRunStatus::Running;
				}

				// Evaluator가 바인딩한 ChaseLocation은 Inner/Outer, 노이즈, DT 오버라이드가
				// 반영된 실제 Move To 목적지다.
				// GetSlotLocation은 Inner 기준만 반환하므로 도착 판정에는 ChaseLocation을 사용한다.
				const bool bReachedSlot = !InstanceData.ChaseLocation.IsNearlyZero()
					&& FVector::DistSquared2D(GatePawn->GetActorLocation(), InstanceData.ChaseLocation)
						<= FMath::Square(InstanceData.ArrivalRadius);
				if (!bReachedSlot)
				{
					InstanceData.ElapsedTime = 0.f;
					return EStateTreeRunStatus::Running;
				}
			}
		}
	}

	// 도착 대기 게이트를 통과한 뒤 StrafeInterval을 누적한다.
	InstanceData.ElapsedTime += DeltaTime;

	if (InstanceData.ElapsedTime < InstanceData.StrafeInterval)
	{
		return EStateTreeRunStatus::Running;
	}

	InstanceData.ElapsedTime = -FMath::FRandRange(0.f, InstanceData.StrafeIntervalJitter);

	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (UCharacterMovementComponent* CharacterMovement = Pawn->FindComponentByClass<UCharacterMovementComponent>())
	{
		ApplyShiftOrbitFacing(Pawn, CharacterMovement, InstanceData.TargetActor);
	}

	UEncirclementSubsystem* EncSubsystem =
		Pawn->GetWorld()->GetSubsystem<UEncirclementSubsystem>();
	if (!EncSubsystem || !IsValid(InstanceData.TargetActor))
	{
		return EStateTreeRunStatus::Running;
	}
	
	// 점유 수가 기준 미만인 1:1 가디언 등은 슬롯을 재배치하지 않고 현재 위치에서 타깃을 바라본다.
	if (EncSubsystem->GetCommittedCount(InstanceData.TargetActor) < InstanceData.MinOccupantsToCircle)
	{
		return EStateTreeRunStatus::Running;
	}
	
	const int32 CurrentSlot = EncSubsystem->GetCurrentSlot(InstanceData.TargetActor, Pawn);
	
	if (CurrentSlot == INDEX_NONE)
	{
		const int32 RequestedSlot = EncSubsystem->RequestSlot(InstanceData.TargetActor, Pawn);
		if (RequestedSlot == INDEX_NONE)
		{
			UE_LOG(LogStateTree, Warning, TEXT("[%s] CurrentSlot not found and slot request failed"), *Pawn->GetName());
			return EStateTreeRunStatus::Running;
		}
	}
	
	const int32 ActiveSlot = EncSubsystem->GetCurrentSlot(InstanceData.TargetActor, Pawn);
	if (ActiveSlot == INDEX_NONE)
	{
		return EStateTreeRunStatus::Running;
	}

	const int32 NumSlots = EncSubsystem->GetNumSlots();
	const int32 Direction = InstanceData.StrafeDirection >= 0 ? 1 : -1;
	const int32 MaxSteps = FMath::Clamp(
		InstanceData.MaxSlotShiftSteps,
		1,
		FMath::Max(1, NumSlots - 1));

	for (int32 Step = 1; Step <= MaxSteps; ++Step)
	{
		const int32 NewSlot = (ActiveSlot + Direction * Step + NumSlots) % NumSlots;

		if (EncSubsystem->ShiftSlotExplicit(InstanceData.TargetActor, Pawn, NewSlot) != INDEX_NONE)
		{
			break;
		}
	}

	return EStateTreeRunStatus::Running;
}

void FStateTreeTask_ShiftOrbitSlot::ExitState(
	FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	
	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	if (!Pawn)
	{
		return;
	}
	
	if (UCharacterMovementComponent* CharacterMovement = Pawn->FindComponentByClass<UCharacterMovementComponent>())
	{
		CharacterMovement->bOrientRotationToMovement = InstanceData.bOriginalOrient;
		CharacterMovement->bUseControllerDesiredRotation = InstanceData.bOriginalControllerRot;
	}

	Pawn->bUseControllerRotationYaw = InstanceData.bOriginalUseControllerRotationYaw;

	if (AAIController* AIC = Pawn->GetController<AAIController>())
	{
		AIC->ClearFocus(EAIFocusPriority::Gameplay);
	}
}
