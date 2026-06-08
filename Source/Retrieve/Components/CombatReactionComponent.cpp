#include "CombatReactionComponent.h"

#include "GameFramework/Actor.h"
#include "Combat/RetrieveHitReactionProfile.h"
#include "Data/LockOnCameraConfig.h"
#include "Data/LockOnConfig.h"
#include "HitReactionComponent.h"
#include "LockOnCameraRig.h"
#include "LockOnComponent.h"
#include "LockOnTargetHighlighter.h"

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
		if (IsValid(LockOnConfig) == false)
		{
			UE_LOG(LogTemp, Warning, TEXT("[ReactionComp] LockOnConfig 미지정."));
		}
		LockOnComp->SetConfig(LockOnConfig);
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
		if (IsValid(LockOnCameraConfig) == false)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[ReactionComp] LockOnCameraConfig 미지정."));
		}
		LockOnCameraRigComp->SetConfig(LockOnCameraConfig);
		LockOnCameraRigComp->Initialize();
	}
	
	if (IsValid(LockOnHighlighter) == false)
	{
		LockOnHighlighter = NewObject<ULockOnTargetHighlighter>(this);
	}
	
	if (IsValid(HitReactionComp) == false)
	{
		HitReactionComp = NewObject<UHitReactionComponent>(Owner, TEXT("HitReactionComp"));
		Owner->AddOwnedComponent(HitReactionComp);
		HitReactionComp->RegisterComponent();
	}
	if (IsValid(HitReactionComp))
	{
		HitReactionComp->Configure(HitReactionProfile);
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
		LockOnCameraRigComp->StopTracking(true);
		LockOnCameraRigComp->DestroyComponent();
		LockOnCameraRigComp = nullptr;
	}
	
	if (IsValid(LockOnHighlighter))
	{
		LockOnHighlighter->Clear();
		LockOnHighlighter = nullptr;
	}

	if (IsValid(HitReactionComp))
	{
		HitReactionComp->DestroyComponent();
		HitReactionComp = nullptr;
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

float UCombatReactionComponent::GetTurnInterpSpeed() const
{
	return LockOnCameraConfig ? LockOnCameraConfig->TurnInterpSpeed : 14.f;
}

bool UCombatReactionComponent::IsLockedOn() const
{
	return LockOnComp && LockOnComp->IsLockedOn();
}

void UCombatReactionComponent::HandleLockOnTargetChanged(AActor* NewTarget)
{
	if (IsValid(LockOnCameraRigComp) == false || IsValid(LockOnHighlighter) == false)
	{
		return;
	}
	
	if (IsValid(NewTarget))
	{
		LockOnCameraRigComp->StartTracking(NewTarget);
		LockOnHighlighter->Apply(NewTarget);
	}
	else
	{
		LockOnCameraRigComp->StopTracking();
		LockOnHighlighter->Clear();
	}
	OnLockOnTargetChanged.Broadcast(NewTarget);
}
