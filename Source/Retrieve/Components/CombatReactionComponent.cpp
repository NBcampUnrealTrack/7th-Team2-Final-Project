

#include "CombatReactionComponent.h"

#include "LockOnCameraRig.h"
#include "Components/LockOnComponent.h"
#include "GameFramework/Actor.h"

UCombatReactionComponent::UCombatReactionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCombatReactionComponent::BeginPlay()
{
	Super::BeginPlay();
	
	AActor* Owner = GetOwner();
	if (IsValid(Owner) == false)
	{
		return;
	}
	
	if (IsValid(LockOnComp) == false)
	{
		LockOnComp = NewObject<ULockOnComponent>(Owner, TEXT("LockOnComp"));
		Owner->AddOwnedComponent(LockOnComp);
		LockOnComp->RegisterComponent();
	}
	
	if (IsValid(LockOnComp))
	{
		LockOnComp->OnTargetChanged.AddUniqueDynamic(this, &UCombatReactionComponent::HandleLockOnTargetChanged);
	}
	
	if (IsValid(LockOnCameraRigComp) == false)
	{
		LockOnCameraRigComp = NewObject<ULockOnCameraRig>(Owner, TEXT("LockOnCameraRigComp"));
		Owner->AddOwnedComponent(LockOnCameraRigComp);
		LockOnCameraRigComp->RegisterComponent();
	}
	
	if (IsValid(LockOnCameraRigComp))
	{
		LockOnCameraRigComp->Initialize();
	}
}

void UCombatReactionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(LockOnComp))
	{
		LockOnComp->OnTargetChanged.RemoveDynamic(this, &UCombatReactionComponent::HandleLockOnTargetChanged);
		LockOnComp->DestroyComponent();
		LockOnComp = nullptr;
	}
	
	if (IsValid(LockOnCameraRigComp))
	{
		LockOnCameraRigComp->StopTracking();
		LockOnCameraRigComp->DestroyComponent();
		LockOnCameraRigComp = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

bool UCombatReactionComponent::TryToggleLockOn()
{
	UE_LOG(LogTemp, Warning, TEXT("[ReactionComp] TryToggleLockOn called, LockOnComp=%s"), 
		LockOnComp ? *LockOnComp->GetName() : TEXT("nullptr"));
	return LockOnComp ? LockOnComp->Toggle() : false;
}

bool UCombatReactionComponent::TrySwitchLockOnTarget(FVector2D InputDir)
{
	return LockOnComp ? LockOnComp->SwitchTarget(InputDir) : false;
}

AActor* UCombatReactionComponent::GetLockOnTarget() const
{
	return LockOnComp ? LockOnComp->GetCurrentTarget() : nullptr;
}

bool UCombatReactionComponent::IsLockedOn() const
{
	return LockOnComp && LockOnComp->IsLockedOn();
}

void UCombatReactionComponent::HandleLockOnTargetChanged(AActor* NewTarget)
{
	if (IsValid(LockOnCameraRigComp) == false)
	{
		return;
	}
	
	if (IsValid(NewTarget))
	{
		LockOnCameraRigComp->StartTracking(NewTarget);
	}
	else
	{
		LockOnCameraRigComp->StopTracking();
	}
	OnLockOnTargetChanged.Broadcast(NewTarget);
}
