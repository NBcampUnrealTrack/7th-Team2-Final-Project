#include "CombatReactionComponent.h"

#include "GameFramework/Actor.h"
#include "Combat/RetrieveHitReactionProfile.h"
#include "Data/LockOnCameraConfig.h"
#include "Data/LockOnConfig.h"
#include "HitReactionComponent.h"
#include "../LockOn/LockOnCameraRig.h"
#include "../LockOn/LockOnComponent.h"
#include "../LockOn/LockOnTargetHighlighter.h"
#include "Components/WidgetComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"

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

	if (IsValid(ReticleWidgetComp))
	{
		ReticleWidgetComp->DestroyComponent();
		ReticleWidgetComp = nullptr;
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
		EnsureReticleComp();
		AttachReticleToTarget(NewTarget);
		if (IsValid(ReticleWidgetComp))
		{
			ReticleWidgetComp->SetVisibility(true);
		}
	}
	else
	{
		LockOnCameraRigComp->StopTracking();
		LockOnHighlighter->Clear();
		if (IsValid(ReticleWidgetComp))
		{
			ReticleWidgetComp->SetVisibility(false);
			ReticleWidgetComp->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		}
	}
	OnLockOnTargetChanged.Broadcast(NewTarget);
}

// 레티클 위젯 컴포넌트 lazy 생성(스크린 공간 — 렌더 파이프라인이 위치 처리, 지터 없음)
void UCombatReactionComponent::EnsureReticleComp()
{
	if (IsValid(ReticleWidgetComp) || IsValid(ReticleWidgetClass) == false)
	{
		return;
	}
	AActor* Owner = GetOwner();
	if (IsValid(Owner) == false)
	{
		return;
	}
	ReticleWidgetComp = NewObject<UWidgetComponent>(Owner);
	ReticleWidgetComp->SetWidgetSpace(EWidgetSpace::Screen);
	ReticleWidgetComp->SetWidgetClass(ReticleWidgetClass);
	ReticleWidgetComp->SetDrawSize(ReticleDrawSize);
	ReticleWidgetComp->SetPivot(ReticlePivot);   // 스크린 픽셀 기준 오프셋(거리 무관)
	ReticleWidgetComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ReticleWidgetComp->SetVisibility(false);
	ReticleWidgetComp->RegisterComponent();
}

// 캡슐 최상단 기준으로 스냅(애니메이션 자세에 안 흔들림). 캡슐 없으면 소켓/바운드 폴백
void UCombatReactionComponent::AttachReticleToTarget(AActor* Target)
{
	if (IsValid(ReticleWidgetComp) == false || IsValid(Target) == false)
	{
		return;
	}

	// 1순위: 캐릭터 캡슐 최상단 + 오프셋 (Z축은 월드 수직 유지 → 자세 무관 안정)
	if (const ACharacter* Char = Cast<ACharacter>(Target))
	{
		if (UCapsuleComponent* Capsule = Char->GetCapsuleComponent())
		{
			ReticleWidgetComp->AttachToComponent(
				Capsule, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			// 0=바닥, 0.5=중심, 1=상단 → 캡슐 로컬 Z (+ 추가 오프셋)
			const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
			const float RatioZ = (ReticleCapsuleHeightRatio * 2.f - 1.f) * HalfHeight;
			ReticleWidgetComp->SetRelativeLocation(
				FVector(ReticleAttachOffset.X, ReticleAttachOffset.Y, RatioZ + ReticleAttachOffset.Z));
			return;
		}
	}

	// 폴백: 캡슐 없는 타겟 → 스켈레탈 메시 소켓, 없으면 바운드 중심
	USkeletalMeshComponent* Mesh = Target->FindComponentByClass<USkeletalMeshComponent>();
	USceneComponent* AttachParent = Mesh ? static_cast<USceneComponent*>(Mesh) : Target->GetRootComponent();
	if (IsValid(AttachParent) == false)
	{
		return;
	}
	const FName Socket = (Mesh && Mesh->DoesSocketExist(LockOnSocketName)) ? LockOnSocketName : NAME_None;

	ReticleWidgetComp->AttachToComponent(
		AttachParent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);

	if (Socket.IsNone())
	{
		FVector Origin, Extent;
		Target->GetActorBounds(true, Origin, Extent);
		ReticleWidgetComp->SetWorldLocation(Origin);
	}
	else
	{
		ReticleWidgetComp->SetRelativeLocation(ReticleAttachOffset);
	}
}
