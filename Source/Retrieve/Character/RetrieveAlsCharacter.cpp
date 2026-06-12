
#include "RetrieveAlsCharacter.h"

#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/CombatReactionComponent.h"
#include "Components/RetrieveCharacterMovementComponent.h"
#include "Components/RetrieveHeroComponent.h"
#include "Components/RetrievePawnExtensionComponent.h"
#include "Field/FieldSystemObjects.h"
#include "GameFramework/Character.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Settings/AlsCharacterSettings.h"
#include "Settings/RetrieveSwimSettings.h"
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

	// Settings DA가 .gitignore 처리되어 팀 공유가 어려운 항목들을 코드에서 강제.
	// 원본 자산을 변경하지 않도록 캐릭터 소유의 인스턴스 사본을 사용.
	if (Settings)
	{
		Settings = DuplicateObject<UAlsCharacterSettings>(Settings, this);
		ApplyRetrieveSettingsOverrides();
	}
}

void ARetrieveAlsCharacter::ApplyRetrieveSettingsOverrides()
{
	if (!Settings) { return; }

	// === 낙법 ===
	// 낙하 Z속도(cm/s) 임계치. h ≈ v²/(2g) 환산:
	//   700  → ≈ 2.5m
	//   1000 → ≈ 5.1m  (현재 기본)
	//   1500 → ≈ 11.5m
	Settings->Rolling.bStartRollingOnLand         = true;
	Settings->Rolling.RollingOnLandSpeedThreshold = 1000.0f;

	// === 낙사(Ragdoll on Land) ===
	// 기본 비활성. 죽음 처리는 GA/wrapper(StartRagdoll)에서 명시적으로 호출.
	Settings->Ragdolling.bStartRagdollingOnLand = false;

	// === Mantle 자동 활성 ===
	// DA 측 토글이 갈팡질팡하지 않도록 코드에 명시.
	Settings->Mantling.bAllowMantling          = true;
	Settings->Mantling.bAutoStartMantlingInAir = true;
}

void ARetrieveAlsCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	const URetrieveAbilitySystemComponent* ASC = GetRetrieveAbilitySystemComponent();
	if (ASC && ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Swimming))
	{
		RefreshSwimmingRotation(DeltaTime);
		return;
	}
	
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
	// 애니메이션 종료 후 타겟으로 Yaw 복귀
	AActor* Target = TurnTarget.Get();
	if (IsValid(Target) == false)
	{
		return;
	}

	const float TargetYaw = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D().Rotation().Yaw;
	const float NewYaw = FMath::RInterpTo(GetActorRotation(), FRotator(0.f, TargetYaw, 0.f), DeltaTime, TurnInterpSpeed).Yaw;
	SetRotationInstant(NewYaw);
	if (FMath::Abs(FMath::FindDeltaAngleDegrees(NewYaw, TargetYaw)) <= 1.f)
	{
		TurnTarget = nullptr;
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
	UCombatReactionComponent* Reaction = FindComponentByClass<UCombatReactionComponent>();
	if (IsValid(Reaction) == false)
	{
		return;
	}

	if (NewCount > 0)
	{
		TurnTarget = nullptr;
		return;
	}

	URetrieveAbilitySystemComponent* ASC = GetRetrieveAbilitySystemComponent();
	const bool bDodging  = ASC && ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Dodging);

	if (GetDesiredRotationMode() == AlsRotationModeTags::ViewDirection && bDodging == false)
	{
		TurnYawTowardActor(Reaction->GetLockOnTarget(), Reaction->GetTurnInterpSpeed());
	}
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

bool ARetrieveAlsCharacter::TryMantleFromWater()
{
	// 수영 climb-out 전용 트레이스. 값은 Project Settings > Retrieve > Swim > ClimbOut.
	return StartMantling(GetDefault<URetrieveSwimSettings>()->SwimClimbOutTrace);
}

bool ARetrieveAlsCharacter::IsMantling() const
{
	return GetLocomotionAction() == AlsLocomotionActionTags::Mantling;
}

void ARetrieveAlsCharacter::StartRagdoll()
{
	StartRagdolling();
}

bool ARetrieveAlsCharacter::StopRagdoll()
{
	return StopRagdolling();
}

void ARetrieveAlsCharacter::BeginRollLockoutTowardYaw(float TargetYawAngle)
{
	// ALS StartRollingImplementation과 동일 패턴.
	// RollingState.TargetYawAngle은 매 프레임 RefreshRollingPhysics가 사용하는 회전 목표.
	// SetRotationInstant는 actor + LocomotionState 일관 갱신.
	TurnTarget = nullptr;
	RollingState.TargetYawAngle = TargetYawAngle;
	SetRotationInstant(TargetYawAngle, ETeleportType::TeleportPhysics);
	SetLocomotionAction(AlsLocomotionActionTags::Rolling);
}

void ARetrieveAlsCharacter::EndRollLockout()
{
	SetLocomotionAction(FGameplayTag::EmptyTag);
	// TEST 락온 중 이면 구르기 종료 즉시 타겟 방향으로 스냅
	UCombatReactionComponent* Reaction = FindComponentByClass<UCombatReactionComponent>();
	if (IsValid(Reaction) == false)
	{
		return;
	}
	if (GetDesiredRotationMode() == AlsRotationModeTags::ViewDirection)
	{
		TurnYawTowardActor(Reaction->GetLockOnTarget(), Reaction->GetTurnInterpSpeed());
	}
}

void ARetrieveAlsCharacter::NotifyLocomotionModeChanged(FGameplayTag PreviousLocomotionMode)
{
	// 착지(InAir→Grounded) 판정 바로 그 시점에서만 낙법을 억제
	// Super가 bStartRollingOnLand + 낙하속도로 낙법을 발동하므로, 
	// 억제 요청 시 그 순간에만 bStartRollingOnLand를 끄고 Super 호출 후 즉시 복구
	const bool bLanding = GetLocomotionMode() == AlsLocomotionModeTags::Grounded
		&& PreviousLocomotionMode == AlsLocomotionModeTags::InAir;

	if (bLanding && bSuppressLandingRoll && Settings)
	{
		const bool bPrev = Settings->Rolling.bStartRollingOnLand;
		Settings->Rolling.bStartRollingOnLand = false;
		Super::NotifyLocomotionModeChanged(PreviousLocomotionMode);
		Settings->Rolling.bStartRollingOnLand = bPrev;

		bSuppressLandingRoll = false; // 1회 소비
		return;
	}

	Super::NotifyLocomotionModeChanged(PreviousLocomotionMode);
}

void ARetrieveAlsCharacter::TurnYawTowardActor(AActor* Target, float InterpSpeed)
{
	if (IsValid(Target) == false)
	{
		return;
	}

	const float TargetYaw = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D().Rotation().Yaw;

	if (InterpSpeed <= 0.f)
	{
		SetRotationInstant(TargetYaw);
		TurnTarget = nullptr;
		return;
	}

	TurnTarget = Target;
	TurnInterpSpeed = InterpSpeed;
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

void ARetrieveAlsCharacter::RefreshSwimmingRotation(float DeltaTime)
{
	const FAlsLocomotionState& LocoState = GetLocomotionState();
	if (!LocoState.bHasVelocity)
	{
		return; // 가속도 없으면 현재 방향을 유지한다.
	}
	const URetrieveSwimSettings* Swim = GetDefault<URetrieveSwimSettings>();
	SetRotationExtraSmooth(LocoState.VelocityYawAngle, DeltaTime, Swim->SwimRotationHalfLife, Swim->SwimRotationSpeed);
}
