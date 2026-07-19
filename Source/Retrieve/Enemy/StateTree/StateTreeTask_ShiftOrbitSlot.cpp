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
	// opt-in=false: 기존 동작 유지 (첫 Tick에서 즉시 만료 상태 → 첫 슬롯 요청 지연 없음)
	// opt-in=true : 도착 후에만 누적한다는 원칙과 충돌하므로 0으로 시작.
	//               최초 슬롯 배정은 아래 Tick 게이트가 별개로 즉시 처리.
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

	// === opt-in 게이팅 (opt-in=false면 이 블록 통째로 스킵) ===
	// opt-in=true 몬스터:
	//   MinOccupants 미달 → 스킵 (기존 정책 보존)
	//   슬롯 없음 → 즉시 RequestSlot + ElapsedTime=0 + 반환 (같은 틱에 Shift하지 않음)
	//   슬롯 있고 미도착 → ElapsedTime=0 리셋 후 반환 (도착 후에만 아래 흐름의 누적 시작)
	// 이 게이트를 ElapsedTime += DeltaTime 이전에 두어 이동 중 실질적으로 누적이 없다.
	if (InstanceData.bWaitForArrivalBeforeShift)
	{
		if (APawn* GatePawn = Context.GetExternalDataPtr(PawnHandle))
		{
			UEncirclementSubsystem* GateEncSub = IsValid(InstanceData.TargetActor)
				? GatePawn->GetWorld()->GetSubsystem<UEncirclementSubsystem>()
				: nullptr;
			if (GateEncSub)
			{
				// MinOccupants 정책은 opt-in 여부와 무관하게 동일하게 보존.
				if (GateEncSub->GetCommittedCount(InstanceData.TargetActor)
					< InstanceData.MinOccupantsToCircle)
				{
					InstanceData.ElapsedTime = 0.f;
					return EStateTreeRunStatus::Running;
				}

				const int32 GateSlot = GateEncSub->GetCurrentSlot(InstanceData.TargetActor, GatePawn);
				if (GateSlot == INDEX_NONE)
				{
					// 최초 슬롯 배정은 도착 여부와 관계없이 즉시 요청.
					// Evaluator가 보통 먼저 배정하지만 Task 자체가 그 사실에 의존하지 않도록.
					const int32 RequestedSlot = GateEncSub->RequestSlot(InstanceData.TargetActor, GatePawn);
					if (RequestedSlot == INDEX_NONE)
					{
						UE_LOG(LogStateTree, Warning, TEXT("[%s] CurrentSlot not found and slot request failed"), *GatePawn->GetName());
					}
					// 새로 배정한 슬롯을 같은 틱에 다시 Shift하지 않는다.
					InstanceData.ElapsedTime = 0.f;
					return EStateTreeRunStatus::Running;
				}

				// ChaseLocation은 Evaluator에서 바인딩된 실제 Move To 목적지
				// (Inner/Outer, 노이즈, DT 오버라이드 모두 반영). GetSlotLocation 재계산은
				// 항상 Inner 값이라 실제 목적지와 어긋나므로 반드시 이 값을 써야 한다.
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

	// === 여기부터 기존 코드 완전 보존 (opt-in=false는 위 블록 스킵 후 여기부터 실행) ===
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
	
	// 소수가 교전 중일 때(1:1 가디언 등)는 자리 재배치가 불필요 → 서클링 생략하고 SetFocus로 바라보며 대기.
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
