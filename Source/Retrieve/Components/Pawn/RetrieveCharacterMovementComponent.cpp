#include "Components/Pawn/RetrieveCharacterMovementComponent.h"

#include "AbilitySystem/Attributes/CombatAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Settings/RetrieveSwimSettings.h"

namespace RetrieveCharacterMovement
{
	static float GroundTraceDistance = 100000.0f;

	FAutoConsoleVariableRef CVarGroundTraceDistance(
		TEXT("RetrieveCharacter.GroundTraceDistance"),
		GroundTraceDistance,
		TEXT("지면 정보를 계산할 때 아래 방향으로 검사할 최대 거리입니다."),
		ECVF_Cheat);
}

URetrieveCharacterMovementComponent::URetrieveCharacterMovementComponent(const FObjectInitializer& ObjectInitializer)
	: Super()   // UAlsCharacterMovementComponent 생성자는 ObjectInitializer 인자를 받지 않음
{
	NavAgentProps.bCanSwim = true;
}

void URetrieveCharacterMovementComponent::SimulateMovement(float DeltaTime)
{
	if (bHasReplicatedAcceleration)
	{
		const FVector OriginalAcceleration = Acceleration;
		Super::SimulateMovement(DeltaTime);
		Acceleration = OriginalAcceleration;
		return;
	}

	Super::SimulateMovement(DeltaTime);
}

bool URetrieveCharacterMovementComponent::CanAttemptJump() const
{
	// Lyra와 동일하게 웅크림 상태는 여기서 막지 않습니다.
	// 점프 가능 여부의 게임 규칙은 Jump Ability와 GameplayTag에서 판단합니다.

	// 액션(공격/가드/대시/강공격/버스트) 진행 중에 점프 금지
	if (const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		if (ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Attacking) ||
			ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Guarding) ||
			ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Dodging) ||
			ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_UsingHeavyAttack) ||
			ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Bursting))
		{
			return false;
		}
	}

	// 완전 잠수(캡슐 상단 < 수면) 시 점프 차단. Wade(머리 위)는 통과 — 3인칭 자유시점이라 카메라 잠김 무관.
	if (bWaterFullySubmerged)
	{
		return false;
	}

	return IsJumpAllowed() &&
		(IsMovingOnGround() || IsFalling());
}

void URetrieveCharacterMovementComponent::NotifySwimEntry()
{
	bPlunging = (Velocity.Z <= -GetDefault<URetrieveSwimSettings>()->PlungeEntrySpeed);
}

void URetrieveCharacterMovementComponent::SetWaterState(float InSurfaceZ, float InSubmersion, bool bInColumn, bool bFullySubmerged)
{
	WaterSurfaceZ = InSurfaceZ;
	WaterSubmersion = InSubmersion;
	bInWaterColumn = bInColumn;
	bWaterFullySubmerged = bFullySubmerged;
}

void URetrieveCharacterMovementComponent::ClearWaterState()
{
	WaterSubmersion = 0.f;
	bInWaterColumn = false;
	bWaterFullySubmerged = false;
}

void URetrieveCharacterMovementComponent::InitializeComponent()
{
	Super::InitializeComponent();
}

const FRetrieveCharacterGroundInfo& URetrieveCharacterMovementComponent::GetGroundInfo()
{
	if (!CharacterOwner || GFrameCounter == CachedGroundInfo.LastUpdateFrame)
	{
		return CachedGroundInfo;
	}

	if (MovementMode == MOVE_Walking)
	{
		CachedGroundInfo.GroundHitResult = CurrentFloor.HitResult;
		CachedGroundInfo.GroundDistance = 0.0f;
	}
	else
	{
		const UCapsuleComponent* CapsuleComp = CharacterOwner->GetCapsuleComponent();
		check(CapsuleComp);

		const float CapsuleHalfHeight = CapsuleComp->GetUnscaledCapsuleHalfHeight();
		const ECollisionChannel CollisionChannel = UpdatedComponent ? UpdatedComponent->GetCollisionObjectType() : ECC_Pawn;
		const FVector TraceStart(GetActorLocation());
		const FVector TraceEnd(
			TraceStart.X,
			TraceStart.Y,
			TraceStart.Z - RetrieveCharacterMovement::GroundTraceDistance - CapsuleHalfHeight);

		FCollisionQueryParams QueryParams(
			SCENE_QUERY_STAT(RetrieveCharacterMovementComponent_GetGroundInfo),
			false,
			CharacterOwner);
		FCollisionResponseParams ResponseParam;
		InitCollisionParams(QueryParams, ResponseParam);

		FHitResult HitResult;
		GetWorld()->LineTraceSingleByChannel(
			HitResult,
			TraceStart,
			TraceEnd,
			CollisionChannel,
			QueryParams,
			ResponseParam);

		CachedGroundInfo.GroundHitResult = HitResult;
		CachedGroundInfo.GroundDistance = RetrieveCharacterMovement::GroundTraceDistance;

		if (MovementMode == MOVE_NavWalking)
		{
			CachedGroundInfo.GroundDistance = 0.0f;
		}
		else if (HitResult.bBlockingHit)
		{
			CachedGroundInfo.GroundDistance = FMath::Max(HitResult.Distance - CapsuleHalfHeight, 0.0f);
		}
	}

	CachedGroundInfo.LastUpdateFrame = GFrameCounter;

	return CachedGroundInfo;
}

void URetrieveCharacterMovementComponent::SetReplicatedAcceleration(const FVector& InAcceleration)
{
	bHasReplicatedAcceleration = true;
	Acceleration = InAcceleration;
}

FRotator URetrieveCharacterMovementComponent::GetDeltaRotation(float DeltaTime) const
{
	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		if (ASC->HasMatchingGameplayTag(RetrieveGameplayTags::Animation_Lock_Rotation))
		{
			return FRotator::ZeroRotator;
		}
	}

	return Super::GetDeltaRotation(DeltaTime);
}

float URetrieveCharacterMovementComponent::GetMaxSpeed() const
{
	if (UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		// 이동 잠금 태그 우선
		if (ASC->HasMatchingGameplayTag(RetrieveGameplayTags::Animation_Lock_Movement))
		{
			return 0.0f;
		}

		// 수영: 단일 속도 + Sprint 가속 (표면/수중 동일). ALS gait는 수영에 안 닿으므로 여기서 직접.
		// Flying 기반이라 IsSwimming()이 false -> 태그로 판별.
		if (ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Swimming))
		{
			const URetrieveSwimSettings* Swim = GetDefault<URetrieveSwimSettings>();
				float Base = Swim->MaxSwimSpeed;
			if (ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Sprinting))
			{
				Base *= Swim->SwimSprintMultiplier;
			}
			if (const UCombatAttributeSet* AttrSet = ASC->GetSet<UCombatAttributeSet>())
			{
				Base *= (AttrSet->GetMoveSpeed() / UCombatAttributeSet::ReferenceMoveSpeed);
			}
			return Base;
		}

		// ALS Gait 기반 베이스 속도에 MoveSpeed Attribute 배율 적용
		// (Attribute 600 = 1.0배, 720 = 1.2배, 300 = 0.5배)
		float AlsBase = Super::GetMaxSpeed();
		if (const UCombatAttributeSet* AttrSet = ASC->GetSet<UCombatAttributeSet>())
		{
			AlsBase *= (AttrSet->GetMoveSpeed() / UCombatAttributeSet::ReferenceMoveSpeed);
		}
		// 수중 보행 항력: 잠수 깊을수록 감속(발끝=1.0 → 완전잠수=WadeMinSpeedMultiplier). Wade/Submerged-Walk에 적용.
		if (bInWaterColumn && WaterSubmersion > 0.f && IsMovingOnGround())
		{
			const URetrieveSwimSettings* Swim = GetDefault<URetrieveSwimSettings>();
			AlsBase *= FMath::Lerp(1.f, Swim->WadeMinSpeedMultiplier, WaterSubmersion);
		}
		return AlsBase;
	}

	return Super::GetMaxSpeed();
}

void URetrieveCharacterMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	bool bSwimTag = false;
	if (const UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()))
	{
		bSwimTag = ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Swimming);
	}

	const bool bSwim = (MovementMode == MOVE_Flying && bSwimTag);
	const URetrieveSwimSettings* Swim = GetDefault<URetrieveSwimSettings>();

	float PreVz = 0.f;
	float InputZ = 0.f;
	if (bSwim) // 수평엔 swim 값 치환(Flying 은닉), 수직은 아래에서 전담
	{
		PreVz = Velocity.Z;
		InputZ = Acceleration.Z;
		Friction = Swim->SwimDrag;
		BrakingDeceleration = Swim->SwimBraking;
	}

	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);

	if (bSwim)
	{
		const float Depth = WaterSurfaceZ - GetActorLocation().Z;

		// 수면 추종 피드포워드(누적 방지: 최종에만 더하고 다음 프레임 고유속도에서 제거).
		const float SurfaceVelZ = (DeltaTime > 0.f)
			? FMath::Clamp((WaterSurfaceZ - PrevWaterSurfaceZ) / DeltaTime, -Swim->MaxSwimSpeed, Swim->MaxSwimSpeed)
			: 0.f;
		const float OwnPreVz = PreVz - PrevSurfaceVelZ; // 직전 FF 제거 → 캐릭터 고유 수직속도

		float VertAccel = InputZ;
		if (InputZ >= 0.f) // 다이브 입력 중엔 부력 억제
		{
			const float Error = FMath::Clamp(Depth - Swim->FloatOffset, -Swim->MaxBuoyancyDepth, Swim->MaxBuoyancyDepth);
			VertAccel += Error * Swim->BuoyancyStiffness - OwnPreVz * Swim->BuoyancyDamping;
		}
		float OwnVz = OwnPreVz + VertAccel * DeltaTime;

		if (OwnVz < 0.f && GetGroundInfo().GroundDistance <= Swim->FloorCapDistance) // 자체 하강만 바닥 캡
		{
			OwnVz = 0.f;
		}
		if (!bPlunging)
		{
			OwnVz = FMath::Clamp(OwnVz, -Swim->MaxSwimSpeed, Swim->MaxSwimSpeed);
		}

		// 표면 오버슈트 차단: 수면 근처에선 상승 모멘텀을 고유속도 + 피드포워드 "둘 다" 감쇠.
		// (OwnVz만 깎으면 SurfaceVelZ가 클램프를 새서 허리춤 위로 솟구침 = 표면 걸림)
		float SurfVzApplied = SurfaceVelZ;
		if (Depth < Swim->SurfaceSoftBand)
		{
			const float Soft = FMath::Clamp(Depth / Swim->SurfaceSoftBand, 0.f, 1.f);
			if (OwnVz > 0.f)         { OwnVz *= Soft; }
			if (SurfVzApplied > 0.f) { SurfVzApplied *= Soft; }
		}

		Velocity.Z = OwnVz + SurfVzApplied;
		PrevWaterSurfaceZ = WaterSurfaceZ;
		PrevSurfaceVelZ = SurfVzApplied; // 실제 적용된 FF 저장 → 다음 프레임 고유속도 복원 일치(누적방지 유지)
	}

	if (bPlunging && Velocity.SizeSquared() <= FMath::Square(Swim->MaxSwimSpeed))
	{
		bPlunging = false;
	}
}

