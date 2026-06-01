
#include "RetrieveAlsCharacter.h"

#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/RetrieveCharacterMovementComponent.h"
#include "Components/RetrieveHeroComponent.h"
#include "Components/RetrievePawnExtensionComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Utility/AlsGameplayTags.h"


ARetrieveAlsCharacter::ARetrieveAlsCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<URetrieveCharacterMovementComponent>(
		ACharacter::CharacterMovementComponentName))
{
	// AAlsCharacter 생성자가 PrimaryActorTick.bCanEverTick = true 로 설정합니다.
	// ALS는 매 프레임 RefreshLocomotion / RefreshView 등을 수행하므로 그대로 유지합니다.

	// 게임 표준: 평시는 이동 방향(액션게임 패턴). 락온/조준 진입은 GAS 태그가 트리거.
	DesiredRotationMode = AlsRotationModeTags::VelocityDirection;

	PawnExtensionComponent = CreateDefaultSubobject<URetrievePawnExtensionComponent>(TEXT("PawnExtensionComponent"));
}

void ARetrieveAlsCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		CachedDefaultHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
	}
}

void ARetrieveAlsCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Crouch 시 ACharacter가 발 위치 유지를 위해 actor.Z를 내림. 그 변화량을 SpringArm Z로 역보정.
	if (USpringArmComponent* SpringArm = GetCameraSpringArm())
	{
		if (UCapsuleComponent* Capsule = GetCapsuleComponent())
		{
			const float CurrentHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
			const float CompensationZ     = CachedDefaultHalfHeight - CurrentHalfHeight;

			FVector Loc = SpringArm->GetRelativeLocation();
			Loc.Z = CompensationZ;
			SpringArm->SetRelativeLocation(Loc);
		}
	}
}

UAbilitySystemComponent* ARetrieveAlsCharacter::GetAbilitySystemComponent() const
{
	return GetRetrieveAbilitySystemComponent();
}

URetrieveAbilitySystemComponent* ARetrieveAlsCharacter::GetRetrieveAbilitySystemComponent() const
{
	return PawnExtensionComponent ? PawnExtensionComponent->GetRetrieveAbilitySystemComponent() : nullptr;
}

void ARetrieveAlsCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	if (PawnExtensionComponent)
	{
		if (DefaultPawnData)
		{
			PawnExtensionComponent->SetPawnData(DefaultPawnData);
			PawnExtensionComponent->HandleControllerChanged();
		}
		InitializeAbilitySystem();
	}
}

void ARetrieveAlsCharacter::UnPossessed()
{
	Super::UnPossessed();
	if (PawnExtensionComponent)
	{
		PawnExtensionComponent->HandleControllerChanged();
		PawnExtensionComponent->UninitializeAbilitySystem();
	}
}

void ARetrieveAlsCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();
	if (PawnExtensionComponent)
	{
		PawnExtensionComponent->HandleControllerChanged();
	}
}

void ARetrieveAlsCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	InitializeAbilitySystem();
	if (PawnExtensionComponent)
	{
		PawnExtensionComponent->HandlePlayerStateReplicated();
	}
}

void ARetrieveAlsCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (URetrieveHeroComponent* HeroComponent = URetrieveHeroComponent::FindHeroComponent(this))
	{
		HeroComponent->InitializePlayerInput(PlayerInputComponent);
	}
	
	if (PawnExtensionComponent)
	{
		PawnExtensionComponent->SetupPlayerInputComponent();
	}
}

void ARetrieveAlsCharacter::InitializeAbilitySystem()
{
	// 베이스는 비어 있음. 자식(ARetrieveAlsCombatCharacter / Test 등)에서
	// PlayerState 소유 ASC 또는 Pawn 소유 ASC를 PawnExtensionComponent에 위임.
}

void ARetrieveAlsCharacter::OnAbilitySystemReady()
{
	URetrieveAbilitySystemComponent* ASC = GetRetrieveAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	// GAS 태그 변화 → ALS 상태 자동 동기화
	ASC->RegisterGameplayTagEvent(
			RetrieveGameplayTags::State_Player_Sprinting,
			EGameplayTagEventType::NewOrRemoved)
	   .AddUObject(this, &ARetrieveAlsCharacter::OnSprintTagChanged);

	ASC->RegisterGameplayTagEvent(
			RetrieveGameplayTags::State_Player_Crouching,
			EGameplayTagEventType::NewOrRemoved)
	   .AddUObject(this, &ARetrieveAlsCharacter::OnCrouchTagChanged);

	ASC->RegisterGameplayTagEvent(
			RetrieveGameplayTags::LockOn_Active,
			EGameplayTagEventType::NewOrRemoved)
	   .AddUObject(this, &ARetrieveAlsCharacter::OnLockOnTagChanged);
}

void ARetrieveAlsCharacter::OnSprintTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	SetDesiredGait(NewCount > 0 ? AlsGaitTags::Sprinting : AlsGaitTags::Running);
}

void ARetrieveAlsCharacter::OnCrouchTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	SetDesiredStance(NewCount > 0 ? AlsStanceTags::Crouching : AlsStanceTags::Standing);
}

void ARetrieveAlsCharacter::OnLockOnTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	SetDesiredRotationMode(NewCount > 0
		? AlsRotationModeTags::ViewDirection
		: AlsRotationModeTags::VelocityDirection);
}

bool ARetrieveAlsCharacter::TryMantle()
{
	return StartMantling();
}

void ARetrieveAlsCharacter::BeginRollLockoutTowardYaw(float TargetYawAngle)
{
	// ALS StartRollingImplementation과 동일 패턴.
	// RollingState.TargetYawAngle은 매 프레임 RefreshRollingPhysics가 사용하는 회전 목표.
	// SetRotationInstant는 actor + LocomotionState 일관 갱신.
	RollingState.TargetYawAngle = TargetYawAngle;
	SetRotationInstant(TargetYawAngle, ETeleportType::TeleportPhysics);
	SetLocomotionAction(AlsLocomotionActionTags::Rolling);
}

void ARetrieveAlsCharacter::EndRollLockout()
{
	SetLocomotionAction(FGameplayTag::EmptyTag);
}

void ARetrieveAlsCharacter::NotifyLocomotionActionChanged(FGameplayTag PreviousLocomotionAction)
{
	Super::NotifyLocomotionActionChanged(PreviousLocomotionAction);

	URetrieveAbilitySystemComponent* ASC = GetRetrieveAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	// ALS LocomotionAction ↔ GAS State 태그 미러링.
	// 진입 시 부여 / 이탈 시 제거. 다른 시스템(콤보/AI 등)이 ASC에서 상태 확인 가능.
	auto SyncTag = [&](const FGameplayTag& AlsTag, const FGameplayTag& GasTag)
	{
		const bool bNowOn = (LocomotionAction == AlsTag);
		const bool bWasOn = (PreviousLocomotionAction == AlsTag);
		if (bNowOn && !bWasOn)
		{
			ASC->AddLooseGameplayTag(GasTag);
		}
		else if (!bNowOn && bWasOn)
		{
			ASC->RemoveLooseGameplayTag(GasTag);
		}
	};

	SyncTag(AlsLocomotionActionTags::Rolling, RetrieveGameplayTags::State_Player_Dodging);
}