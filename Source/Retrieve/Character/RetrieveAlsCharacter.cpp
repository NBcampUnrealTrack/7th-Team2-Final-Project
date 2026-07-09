
#include "RetrieveAlsCharacter.h"

#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/CombatAttributeSet.h"
#include "Components/CapsuleComponent.h"
#include "Components/Combat/CombatReactionComponent.h"
#include "Components/Pawn/RetrieveCharacterMovementComponent.h"
#include "Components/Player/RetrieveHeroComponent.h"
#include "Components/Pawn/RetrievePawnExtensionComponent.h"
#include "Field/FieldSystemObjects.h"
#include "GameFramework/Character.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameplayEffect.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Logging/RetrieveLogChannels.h"
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
	ViewMode = AlsViewModeTags::ThirdPerson;
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
			RetrieveGameplayTags::State_Player_Aiming,
			EGameplayTagEventType::NewOrRemoved)
	   .AddUObject(this, &ARetrieveAlsCharacter::OnAimingTagChanged);

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

void ARetrieveAlsCharacter::OnAimingTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	SetDesiredAiming(NewCount > 0);
}

void ARetrieveAlsCharacter::OnLockOnTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	SetDesiredRotationMode(NewCount > 0
		? AlsRotationModeTags::ViewDirection
		: AlsRotationModeTags::VelocityDirection);
}

bool ARetrieveAlsCharacter::TryMantle()
{
	if (GetLocomotionMode() == AlsLocomotionModeTags::Grounded)
	{
		return StartMantling(Settings->Mantling.GroundedTrace);
	}

	if (GetLocomotionMode() == AlsLocomotionModeTags::InAir)
	{
		return StartMantling(Settings->Mantling.InAirTrace);
	}

	return false;
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

bool ARetrieveAlsCharacter::IsLocomotionActionActive() const
{
	return GetLocomotionAction().IsValid();
}

bool ARetrieveAlsCharacter::RefreshCustomGroundedMovingRotation(float /*DeltaTime*/)
{
	return RefreshHeldFacing();
}

bool ARetrieveAlsCharacter::RefreshCustomGroundedNotMovingRotation(float /*DeltaTime*/)
{
	return RefreshHeldFacing();
}

bool ARetrieveAlsCharacter::RefreshHeldFacing()
{
	if (!bHoldFacing)
	{
		return false;   // 평소엔 ALS 기본 회전
	}

	if (GetLocomotionState().bHasInput)
	{
		bHoldFacing = false;   // 플레이어가 이동을 주면 해제 → 정상 로코모션 복귀
		return false;
	}

	// 회전하지 않고 현재 facing 유지 → ALS의 velocity(후방 포함) 기반 회전을 스킵
	RefreshTargetYawAngleUsingActorRotation();
	return true;
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

void ARetrieveAlsCharacter::NotifyLocomotionModeChanged(const FGameplayTag& PreviousLocomotionMode)
{
	// 착지(InAir→Grounded) 판정 바로 그 시점에서만 낙법을 억제
	// Super가 bStartRollingOnLand + 낙하속도로 낙법을 발동하므로, 
	// 억제 요청 시 그 순간에만 bStartRollingOnLand를 끄고 Super 호출 후 즉시 복구
	const bool bLanding = GetLocomotionMode() == AlsLocomotionModeTags::Grounded
		&& PreviousLocomotionMode == AlsLocomotionModeTags::InAir;

	if (bLanding)
	{
		// 착지 순간 하강 속도(양수 변환). ALS 낙법 판정과 동일 시점/소스(LocomotionState.Velocity.Z).
		const float FallSpeed = -LocomotionState.Velocity.Z;
		HandleLandingImpact(FallSpeed);
	}

	if (bLanding && bSuppressLandingRoll && Settings)
	{
		const bool bPrevRoll = Settings->Rolling.bStartRollingOnLand;
		const bool bPrevRagdoll = Settings->Ragdolling.bStartRagdollingOnLand;
		Settings->Rolling.bStartRollingOnLand = false;
		Settings->Ragdolling.bStartRagdollingOnLand = false;
		Super::NotifyLocomotionModeChanged(PreviousLocomotionMode);
		Settings->Rolling.bStartRollingOnLand = bPrevRoll;
		Settings->Ragdolling.bStartRagdollingOnLand = bPrevRagdoll;

		bSuppressLandingRoll = false; // 1회 소비
		return;
	}

	Super::NotifyLocomotionModeChanged(PreviousLocomotionMode);
}

void ARetrieveAlsCharacter::HandleLandingImpact(float FallSpeed)
{
	// 깊은 물(수영=MOVE_Flying)은 Grounded 착지가 안 떠서 이 함수 자체가 안 불린다 → 물 가드 불필요.
	// 얕은 물(Wade=Walking)은 땅처럼 낙하 데미지가 들어간다(의도).
	if (FallSpeed < FallDamageStartSpeed)
	{
		return; // 착지/낙법은 ALS 기본에 위임
	}

	// leap attack 등 의도된 착지(GA가 착지 전에 SetSuppressLandingRoll을 예약)는 자체 몽타주가
	// 착지 연출을 소유한다. 진입 시점에 이미 켜져 있으면 그 경우 → Landing 몽타주만 스킵(겹침 방지).
	// 낙하 데미지는 그대로 적용한다(높은 곳 leap attack이면 죽어야 하므로).
	const bool bIntendedLanding = bSuppressLandingRoll;

	// 낙법 실패 구간: 낙법 억제 + 낙하 데미지
	SetSuppressLandingRoll(true);
	ApplyFallDamage(FallSpeed);

	// 데미지로 죽지 않았고 의도된 착지가 아니면 낙법 실패 착지 몽타주(비틀거림/경직).
	// 죽었으면(HP<=0) GA_Die가 사망 Ragdoll을 소유하므로 여기선 스킵한다.
	if (HasAuthority() && !bIntendedLanding)
	{
		const URetrieveAbilitySystemComponent* ASC = GetRetrieveAbilitySystemComponent();
		const float Health = ASC ? ASC->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute()) : 0.f;
		if (Health > 0.f && LandingFailMontage)
		{
			PlayAnimMontage(LandingFailMontage, LandingFailMontagePlayRate);
		}
	}

	UE_LOG(LogRetrieveCombat, Log, TEXT("[Fall] Impact FallSpeed=%.0f (threshold=%.0f)"),
		FallSpeed, FallDamageStartSpeed);
}

void ARetrieveAlsCharacter::ApplyFallDamage(float FallSpeed)
{
	if (!HasAuthority())
	{
		return; // Health는 서버 권위 — 서버에서만 적용
	}

	URetrieveAbilitySystemComponent* ASC = GetRetrieveAbilitySystemComponent();
	if (!ASC || !FallDamageEffect)
	{
		return;
	}

	// 곡선(방식 B): 임계 초과분에 비례. 즉사 라인 없음 — 데미지가 현재 HP를 넘으면 사망.
	const float Damage = (FallSpeed - FallDamageStartSpeed) * FallDamageScale;
	if (Damage <= 0.f)
	{
		return;
	}

	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddInstigator(this, this);

	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(FallDamageEffect, 1.f, Context);
	if (Spec.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Damage_Fall, Damage);
		ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);

		UE_LOG(LogRetrieveCombat, Log, TEXT("[Fall] Damage=%.1f (FallSpeed=%.0f)"), Damage, FallSpeed);
	}
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

void ARetrieveAlsCharacter::NotifyLocomotionActionChanged(const FGameplayTag& PreviousLocomotionAction)
{
	Super::NotifyLocomotionActionChanged(PreviousLocomotionAction);

	// State.Player.Dodging은 GA_Dash의 ActivationOwnedTags로만 관리한다.
	// (ALS Rolling 미러링은 어빌리티 종료 경로와 윈도우가 어긋나 피격 시 태그가 잔존하는 문제가 있어 제거)
	
	if (AlsCharacterMovement)
	{
		// 프로젝트 운용 규칙:
		// LocomotionAction은 Retrieve에서 단순한 "공격 상태 표시"가 아니라
		// CharacterMovement 입력 차단 요청으로 취급한다.
		//
		// RootMotion이 있는 공격 몽타주는 애니메이션이 이동과 방향을 직접 소유하므로,
		// Als.LocomotionAction.Attack Notify를 추가로 두지 않는다.
		// 두 시스템을 겹치면 입력 복구, 캔슬 윈도우, 콤보 전환, 착지 블렌딩 타이밍을
		// 추적하기 어려워진다.
		//
		// RootMotion이 없는 공격 몽타주는 CharacterMovement가 여전히 플레이어 입력을
		// 소비할 수 있으므로, 필요한 경우 Als.LocomotionAction.Attack Notify를 사용해
		// 이 경로로 입력을 차단한다.
		//
		// Sliding은 ALS 고유 이동 액션이므로 프로젝트 입력 차단 대상에서 제외한다.
		const bool bBlock = LocomotionAction.IsValid() && LocomotionAction != AlsLocomotionActionTags::Sliding;
		AlsCharacterMovement->SetInputBlocked(bBlock);
	}
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
