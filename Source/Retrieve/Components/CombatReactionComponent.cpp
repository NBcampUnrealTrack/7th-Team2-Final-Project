

#include "CombatReactionComponent.h"
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
	
	if (LockOnComp)
	{
		LockOnComp->OnTargetChanged.AddUniqueDynamic(this, &UCombatReactionComponent::HandleLockOnTargetChanged);
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
	Super::EndPlay(EndPlayReason);
}

bool UCombatReactionComponent::TryToggleLockOn()
{
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
	OnLockOnTargetChanged.Broadcast(NewTarget);
}
