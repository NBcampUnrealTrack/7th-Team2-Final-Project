#include "Components/RetrieveCharacterMovementComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameplayTags/RetrieveGameplayTags.h"

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
	: Super(ObjectInitializer)
{
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
	return IsJumpAllowed() &&
		(IsMovingOnGround() || IsFalling());
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
		if (ASC->HasMatchingGameplayTag(RetrieveGameplayTags::Animation_Lock_Movement))
		{
			return 0.0f;
		}
	}

	return Super::GetMaxSpeed();
}
