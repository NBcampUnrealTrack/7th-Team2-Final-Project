#include "Enemy/StateTree/StateTreeTask_ShiftOrbitSlot.h"

#include "StateTreeLinker.h"
#include "StateTreeExecutionContext.h"
#include "Components/Enemy/EnemyCombatComponent.h"
#include "Enemy/EncirclementSubsystem.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AIController.h"

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

	Pawn->bUseControllerRotationYaw = true;
	CharacterMovement->bOrientRotationToMovement = false;
	CharacterMovement->bUseControllerDesiredRotation = true;
	
	
	AAIController* AIC = Pawn->GetController<AAIController>();
	if (IsValid(AIC) && IsValid(InstanceData.TargetActor))
	{
		AIC->SetFocus(InstanceData.TargetActor, EAIFocusPriority::Gameplay);
	}
	
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
	
	InstanceData.ElapsedTime = 0.f;
	
	APawn* Pawn = Context.GetExternalDataPtr(PawnHandle);
	if (!Pawn)
	{
		return EStateTreeRunStatus::Failed;
	}

	if (AAIController* AIC = Pawn->GetController<AAIController>())
	{
		if (IsValid(InstanceData.TargetActor))
		{
			AIC->SetFocus(InstanceData.TargetActor, EAIFocusPriority::Gameplay);
		}
	}

	UEncirclementSubsystem* EncSubsystem =
		Pawn->GetWorld()->GetSubsystem<UEncirclementSubsystem>();
	if (!EncSubsystem || !IsValid(InstanceData.TargetActor))
	{
		return EStateTreeRunStatus::Running;
	}
	
	const int32 CurrentSlot = EncSubsystem->GetCurrentSlot(InstanceData.TargetActor, Pawn);
	
	if (CurrentSlot == INDEX_NONE)
	{
		UE_LOG(LogStateTree, Error, TEXT("[%s] CurrentSlot not found"), *Pawn->GetName());
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
		const int32 NewSlot = (CurrentSlot + Direction * Step + NumSlots) % NumSlots;

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
