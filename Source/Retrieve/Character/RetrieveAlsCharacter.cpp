
#include "RetrieveAlsCharacter.h"

#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/CombatAttributeSet.h"
#include "AlsAnimationInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/Combat/CombatReactionComponent.h"
#include "Components/Pawn/RetrieveCharacterMovementComponent.h"
#include "Components/Player/RetrieveHeroComponent.h"
#include "Components/Pawn/RetrievePawnExtensionComponent.h"
#include "Field/FieldSystemObjects.h"
#include "GameFramework/Character.h"
#include "GameFramework/SpringArmComponent.h"
#include "TimerManager.h"
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
	// ALS 부모 Tick은 AnimationInstance 캐시(TWeakObjectPtr, PostInitializeComponents에서 1회 캐싱)가
	// 무효면 조용히 조기 리턴한다 → RefreshView 정지 = 카메라(뷰 회전) 영구 동결.
	// 시퀀서 종료 복원 등으로 메인 메시의 애님 인스턴스가 재생성되면 캐시가 옛 인스턴스를 가리키다
	// GC 시점에 갑자기 무효화되므로("어느 순간부터 카메라 고정" 버그), 매 틱 저렴하게 재검증해 자기 치유한다.
	if (!AnimationInstance.IsValid())
	{
		AnimationInstance = Cast<UAlsAnimationInstance>(GetMesh()->GetAnimInstance());

		// 자기 치유 흔적(드물게 발생 — 시퀀서/GC로 옛 인스턴스가 수거된 직후). 기본 출력엔 안 뜸.
		UE_LOG(LogRetrieveWorld, Verbose,
		       TEXT("ALS AnimationInstance 캐시 무효 → 재캐싱 (%s)"),
		       *GetNameSafe(AnimationInstance.Get()));
	}

	Super::Tick(DeltaTime);

	const URetrieveAbilitySystemComponent* ASC = GetRetrieveAbilitySystemComponent();
	if (ASC && ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Swimming))
	{
		bTrackingFall = false; // 수영 중엔 낙차 추적 중단 — 물이 낙하를 끊는다(입수→탈출 시 오탐 방지).
		RefreshSwimmingRotation(DeltaTime);
		return;
	}

	// 낙하 데미지용: 공중에 있는 동안 최고점 Z(정점)를 기록한다. 착지 시 (정점 - 착지Z)로
	// 실제 낙차를 계산 → 속도 대신 높이 기반이라 종단속도 클램프/넉백 런치에도 정확하다.
	if (GetLocomotionMode() == AlsLocomotionModeTags::InAir)
	{
		const float CurrentZ = GetActorLocation().Z;
		FallApexZ = bTrackingFall ? FMath::Max(FallApexZ, CurrentZ) : CurrentZ;
		bTrackingFall = true;
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

bool ARetrieveAlsCharacter::IsMantlingAllowedToStart_Implementation() const
{
	// Desired가 아닌 실제 Stance 기준 — 크라우치 해제 직후 일어나는 중 프레임에 멘틀이 끼는 것을 막는다.
	return Super::IsMantlingAllowedToStart_Implementation()
		&& GetStance() != AlsStanceTags::Crouching;
}

void ARetrieveAlsCharacter::OnMantlingStarted_Implementation(const FAlsMantlingParameters& Parameters)
{
	Super::OnMantlingStarted_Implementation(Parameters);

	MantleTargetPrimitive = Parameters.TargetPrimitive;
}

void ARetrieveAlsCharacter::OnMantlingEnded_Implementation()
{
	Super::OnMantlingEnded_Implementation();

	ResolveMantlePenetrationUpward();
	MantleTargetPrimitive.Reset();
}

void ARetrieveAlsCharacter::ResolveMantlePenetrationUpward()
{
	// 시뮬레이티드 프록시는 복제 트랜스폼을 따라가므로 직접 보정하지 않는다.
	if (GetLocalRole() <= ROLE_SimulatedProxy)
	{
		return;
	}

	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	UWorld* World = GetWorld();
	if (!IsValid(Capsule) || !World)
	{
		return;
	}

	const float CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	const FCollisionShape CapsuleShape =
		FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), CapsuleHalfHeight);
	const ECollisionChannel Channel = Capsule->GetCollisionObjectType();
	const FCollisionQueryParams QueryParams{TEXT("ResolveMantlePenetrationUpward"), false, this};

	const FVector Location = GetActorLocation();
	if (!World->OverlapBlockingTestByChannel(Location, FQuat::Identity, Channel, CapsuleShape, QueryParams))
	{
		return;
	}

	// 탐색 상한: 멘틀 대상 메시 Bounds 상단 + 캡슐. 대상이 무효면 고정 상한으로 폴백.
	float MaxZ = Location.Z + 300.f;
	if (const UPrimitiveComponent* Target = MantleTargetPrimitive.Get())
	{
		MaxZ = FMath::Max(MaxZ, Target->Bounds.Origin.Z + Target->Bounds.BoxExtent.Z + CapsuleHalfHeight);
	}

	// XY 고정, 위로 계단식 탐색 — FindTeleportSpot은 방향을 안 가려 빈 메시 내부로 밀 수 있으므로 상향만 본다.
	const float StepSize = CapsuleHalfHeight * 0.5f;
	for (float CandidateZ = Location.Z + StepSize; CandidateZ <= MaxZ; CandidateZ += StepSize)
	{
		FVector Candidate{Location.X, Location.Y, CandidateZ};
		if (World->OverlapBlockingTestByChannel(Candidate, FQuat::Identity, Channel, CapsuleShape, QueryParams))
		{
			continue;
		}

		// 자유공간 발견 — 아래로 스윕해 표면 위에 스냅 (공중에 뜨지 않게)
		FHitResult FloorHit;
		if (World->SweepSingleByChannel(FloorHit, Candidate, FVector{Location.X, Location.Y, Location.Z},
		                                FQuat::Identity, Channel, CapsuleShape, QueryParams))
		{
			Candidate.Z = FloorHit.Location.Z + UCharacterMovementComponent::MIN_FLOOR_DIST;
		}

		SetActorLocation(Candidate, false, nullptr, ETeleportType::TeleportPhysics);
		return;
	}

	// 상한까지 자유공간을 못 찾음 — 엔진 FindTeleportSpot 폴백 (안 하는 것보단 낫다)
	TeleportTo(Location, GetActorRotation());
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

	// 이륙(Grounded→InAir): 낙차 추적 시작점을 현재 Z로 초기화한다. 이후 Tick이 정점을 갱신.
	if (GetLocomotionMode() == AlsLocomotionModeTags::InAir
		&& PreviousLocomotionMode == AlsLocomotionModeTags::Grounded)
	{
		FallApexZ = GetActorLocation().Z;
		bTrackingFall = true;
	}

	if (bLanding)
	{
		// 실제 낙차(정점→착지, cm). 공중에서 Tick이 기록한 최고점 Z 사용.
		// 속도 대신 높이 → 종단속도 클램프/넉백 런치에도 정확.
		const float FallHeight = bTrackingFall
			? FMath::Max(0.f, FallApexZ - GetActorLocation().Z)
			: 0.f;
		bTrackingFall = false;
		HandleLandingImpact(FallHeight);
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

void ARetrieveAlsCharacter::HandleLandingImpact(float FallHeight)
{
	// 블링크 착지: 메시 복원 + 중력 원복 + 낙하 데미지·낙법 억제.
	if (bBlinkActive)
	{
		EndBlink();
		SetSuppressLandingRoll(true);
		return;
	}

	// 깊은 물(수영=MOVE_Flying)은 Grounded 착지가 안 떠서 이 함수 자체가 안 불린다 → 물 가드 불필요.
	// 얕은 물(Wade=Walking)은 땅처럼 낙하 데미지가 들어간다(의도).
	if (FallHeight < FallDamageStartHeight)
	{
		return; // 착지/낙법은 ALS 기본에 위임
	}

	// leap attack 등 의도된 착지(GA가 착지 전에 SetSuppressLandingRoll을 예약)는 자체 몽타주가
	// 착지 연출을 소유한다. 진입 시점에 이미 켜져 있으면 그 경우 → Landing 몽타주만 스킵(겹침 방지).
	// 낙하 데미지는 그대로 적용한다(높은 곳 leap attack이면 죽어야 하므로).
	const bool bIntendedLanding = bSuppressLandingRoll;

	// 낙법 실패 구간: 낙법 억제 + 낙하 데미지
	SetSuppressLandingRoll(true);
	ApplyFallDamage(FallHeight);

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

	UE_LOG(LogRetrieveCombat, Log, TEXT("[Fall] Impact FallHeight=%.0f (threshold=%.0f)"),
		FallHeight, FallDamageStartHeight);
}

void ARetrieveAlsCharacter::ApplyFallDamage(float FallHeight)
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

	// 곡선: 임계 초과 낙차에 비례(높이 선형 → 속도의 제곱). 즉사 라인 없음 — 데미지가 현재 HP를 넘으면 사망.
	const float Damage = (FallHeight - FallDamageStartHeight) * FallDamageHeightScale;
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

		UE_LOG(LogRetrieveCombat, Log, TEXT("[Fall] Damage=%.1f (FallHeight=%.0f)"), Damage, FallHeight);
	}
}

void ARetrieveAlsCharacter::BeginBlink(float MaxDuration)
{
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetVisibility(false, /*bPropagateToChildren=*/true);
	}
	bBlinkActive = true;

	GetWorldTimerManager().ClearTimer(BlinkTimer);
	if (MaxDuration > 0.f)
	{
		// 착지가 안 잡히는 경우(허공 등) 대비 안전 복원.
		GetWorldTimerManager().SetTimer(BlinkTimer, this, &ARetrieveAlsCharacter::EndBlink, MaxDuration, false);
	}
}

void ARetrieveAlsCharacter::SetBlinkFallGravity(float GravityScale)
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		if (!bBlinkGravityBoosted)
		{
			SavedGravityScale = Movement->GravityScale; // 원래 값은 최초 부스트 때만 저장
			bBlinkGravityBoosted = true;
		}
		Movement->GravityScale = GravityScale;
	}
}

void ARetrieveAlsCharacter::EndBlink()
{
	GetWorldTimerManager().ClearTimer(BlinkTimer);

	if (bBlinkGravityBoosted)
	{
		if (UCharacterMovementComponent* Movement = GetCharacterMovement())
		{
			Movement->GravityScale = SavedGravityScale;
		}
		bBlinkGravityBoosted = false;
	}

	if (bBlinkActive)
	{
		if (USkeletalMeshComponent* MeshComp = GetMesh())
		{
			MeshComp->SetVisibility(true, /*bPropagateToChildren=*/true);
		}
		bBlinkActive = false;
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
