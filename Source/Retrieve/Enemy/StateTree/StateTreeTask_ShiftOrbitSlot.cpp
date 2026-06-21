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
		Pawn->bUseControllerRotationYaw = bFaceTarget;
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
	InstanceData.ElapsedTime = InstanceData.StrafeInterval;
	
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
