#include "Character/RetrieveEpicMonsterCharacter.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/Enemy/NormalMonsterHealthBarComponent.h"
#include "Components/Enemy/EpicMonsterGroggyComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "UObject/UnrealType.h"

namespace
{
	constexpr int32 EpicTurnLeftDirection = -1;
	constexpr int32 EpicTurnRightDirection = 1;

	void SetAnimFloatProperty(UAnimInstance* AnimInstance, const FName PropertyName, const float Value)
	{
		if (FFloatProperty* FloatProperty = FindFProperty<FFloatProperty>(AnimInstance->GetClass(), PropertyName))
		{
			FloatProperty->SetPropertyValue_InContainer(AnimInstance, Value);
		}
	}

	void SetAnimBoolProperty(UAnimInstance* AnimInstance, const FName PropertyName, const bool bValue)
	{
		if (FBoolProperty* BoolProperty = FindFProperty<FBoolProperty>(AnimInstance->GetClass(), PropertyName))
		{
			BoolProperty->SetPropertyValue_InContainer(AnimInstance, bValue);
		}
	}

	float CalculateMovementDirectionDegrees(const APawn* Pawn, const FVector& Velocity2D)
	{
		if (!Pawn || Velocity2D.IsNearlyZero())
		{
			return 0.f;
		}

		const FRotator ActorRotation(0.f, Pawn->GetActorRotation().Yaw, 0.f);
		const FVector Forward = FRotationMatrix(ActorRotation).GetUnitAxis(EAxis::X);
		const FVector Right = FRotationMatrix(ActorRotation).GetUnitAxis(EAxis::Y);
		const FVector MoveDirection = Velocity2D.GetSafeNormal();
		return FMath::RadiansToDegrees(FMath::Atan2(
			FVector::DotProduct(Right, MoveDirection),
			FVector::DotProduct(Forward, MoveDirection)));
	}

	void SetMovementDrivenChaseTag(
		UAbilitySystemComponent* ASC,
		bool& bTagAddedByMovement,
		const bool bShouldChase)
	{
		if (!ASC)
		{
			bTagAddedByMovement = false;
			return;
		}

		const FGameplayTag ChaseTag = RetrieveGameplayTags::State_Enemy_Chase;
		if (bShouldChase)
		{
			if (!ASC->HasMatchingGameplayTag(ChaseTag))
			{
				ASC->AddLooseGameplayTag(ChaseTag);
				bTagAddedByMovement = true;
			}
			return;
		}

		if (bTagAddedByMovement)
		{
			ASC->RemoveLooseGameplayTag(ChaseTag);
			bTagAddedByMovement = false;
		}
	}
}

ARetrieveEpicMonsterCharacter::ARetrieveEpicMonsterCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	EpicGroggyComponent = CreateDefaultSubobject<UEpicMonsterGroggyComponent>(TEXT("EpicMonsterGroggyComponent"));
}

void ARetrieveEpicMonsterCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (const UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		const FVector ActorLocation = GetActorLocation();
		FVector Movement2D(MoveComp->Velocity.X, MoveComp->Velocity.Y, 0.f);
		if (bHasAnimMovementSampleLocation && DeltaSeconds > KINDA_SMALL_NUMBER)
		{
			const FVector DeltaLocation2D(
				ActorLocation.X - LastAnimMovementSampleLocation.X,
				ActorLocation.Y - LastAnimMovementSampleLocation.Y,
				0.f);
			const FVector LocationDeltaVelocity2D = DeltaLocation2D / DeltaSeconds;
			if (LocationDeltaVelocity2D.SizeSquared() > Movement2D.SizeSquared())
			{
				Movement2D = LocationDeltaVelocity2D;
			}
		}
		LastAnimMovementSampleLocation = ActorLocation;
		bHasAnimMovementSampleLocation = true;

		const float GroundSpeed = Movement2D.Size();
		const bool bMovingOnGround = GroundSpeed > TurnAnimMovingSpeedThreshold
			&& !MoveComp->IsFalling()
			&& MoveComp->MovementMode != MOVE_Flying;

		const bool bMovementShouldDriveChase = bMovingOnGround
			&& !IsAttackingForAnim()
			&& !IsSpecialAttackingForAnim()
			&& !IsHitForAnim()
			&& !IsStaggeredForAnim()
			&& !IsGroggyForAnim()
			&& !IsDeadForAnim();
		SetMovementDrivenChaseTag(OwnedASC.Get(), bMovementDrivenChaseTagAdded, bMovementShouldDriveChase);

		if (USkeletalMeshComponent* MeshComp = GetMesh())
		{
			if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
			{
				SetAnimFloatProperty(AnimInstance, TEXT("Speed"), GroundSpeed);
				SetAnimFloatProperty(AnimInstance, TEXT("GroundSpeed"), GroundSpeed);
				SetAnimFloatProperty(AnimInstance, TEXT("Direction"), CalculateMovementDirectionDegrees(this, Movement2D));
				SetAnimBoolProperty(AnimInstance, TEXT("IsMoving"), bMovingOnGround);
				SetAnimBoolProperty(AnimInstance, TEXT("bIsMoving"), bMovingOnGround);
				SetAnimBoolProperty(AnimInstance, TEXT("ShouldMove"), bMovingOnGround);
				SetAnimBoolProperty(AnimInstance, TEXT("bShouldMove"), bMovingOnGround);
				SetAnimBoolProperty(AnimInstance, TEXT("IsChasing"), bMovingOnGround);
				SetAnimBoolProperty(AnimInstance, TEXT("bIsChasing"), bMovingOnGround);
			}
		}

		if (bMovementShouldDriveChase)
		{
			if (GroundTurnMontage)
			{
				StopLocomotionMontages();
			}

			if (!ForcedGroundMoveAnimation.IsNull())
			{
				if (USkeletalMeshComponent* MeshComp = GetMesh())
				{
					if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
					{
						const bool bMoveMontagePlaying = GroundMoveMontage
							&& AnimInstance->Montage_IsPlaying(GroundMoveMontage);
						if (!bMoveMontagePlaying)
						{
							UAnimSequenceBase* MoveSequence = ForcedGroundMoveAnimation.LoadSynchronous();
							if (MoveSequence)
							{
								GroundMoveMontage = AnimInstance->PlaySlotAnimationAsDynamicMontage(
									MoveSequence,
									TurnAnimationSlot,
									0.12f,
									0.12f,
									ForcedGroundMovePlayRate,
									999);
							}
						}
					}
				}
			}
		}
		else if (GroundMoveMontage)
		{
			StopLocomotionMontages();
		}
	}
}

void ARetrieveEpicMonsterCharacter::ConfigureEnemyMovement()
{
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (!MoveComp)
	{
		return;
	}

	if (bUseForwardLocomotion)
	{
		bUseControllerRotationYaw = false;
		MoveComp->bUseControllerDesiredRotation = false;
		MoveComp->bOrientRotationToMovement = true;
	}

	MoveComp->RotationRate = FRotator(0.f, RotationRateYaw, 0.f);
	MoveComp->MaxAcceleration = MaxAcceleration;
	MoveComp->BrakingDecelerationWalking = BrakingDeceleration;
	MoveComp->GroundFriction = GroundFriction;
	MoveComp->bUseRVOAvoidance = bUseRVOAvoidance;

	if (CapsuleRadius > 0.f && CapsuleHalfHeight > 0.f)
	{
		if (UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
		{
			CapsuleComp->SetCapsuleSize(CapsuleRadius, CapsuleHalfHeight, true);
		}
	}

	if (bOverrideMeshRelativeZ)
	{
		if (USkeletalMeshComponent* MeshComp = GetMesh())
		{
			FVector RelativeLocation = MeshComp->GetRelativeLocation();
			RelativeLocation.Z = MeshRelativeZ;
			MeshComp->SetRelativeLocation(RelativeLocation);
			InitialMeshRelativeTransform = MeshComp->GetRelativeTransform();
		}
	}

	if (HealthBarHeightOffset > 0.f && NormalHealthBarComponent)
	{
		NormalHealthBarComponent->SetRelativeLocation(FVector(0.f, 0.f, HealthBarHeightOffset));
	}
}

void ARetrieveEpicMonsterCharacter::UpdateGroundTurnAnimation(float SignedYawDelta)
{
	// 턴 애니메이션 에셋이 지정되지 않은 에픽 몬스터는 턴 애니를 사용하지 않는다.
	if (TurnRightAnimation.IsNull() && TurnLeftAnimation.IsNull())
	{
		return;
	}

	if (FMath::Abs(SignedYawDelta) < TurnAnimMinYaw)
	{
		StopLocomotionMontages();
		return;
	}

	if (const UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		if (MoveComp->Velocity.Size2D() > TurnAnimMovingSpeedThreshold || MoveComp->IsFalling())
		{
			StopLocomotionMontages();
			return;
		}
	}

	USkeletalMeshComponent* MeshComp = GetMesh();
	UAnimInstance* AnimInstance = MeshComp ? MeshComp->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return;
	}

	const int32 DesiredDirection = SignedYawDelta > 0.f
		? EpicTurnRightDirection
		: EpicTurnLeftDirection;

	if (GroundTurnMontage && GroundTurnDirection == DesiredDirection
		&& AnimInstance->Montage_IsPlaying(GroundTurnMontage))
	{
		return;
	}

	StopLocomotionMontages();

	const TSoftObjectPtr<UAnimSequenceBase>& DesiredAnimation = DesiredDirection == EpicTurnRightDirection
		? TurnRightAnimation
		: TurnLeftAnimation;

	UAnimSequenceBase* TurnSequence = DesiredAnimation.LoadSynchronous();
	if (!TurnSequence)
	{
		return;
	}

	GroundTurnMontage = AnimInstance->PlaySlotAnimationAsDynamicMontage(
		TurnSequence,
		TurnAnimationSlot,
		0.12f,
		0.12f,
		1.0f,
		999);
	GroundTurnDirection = GroundTurnMontage ? DesiredDirection : 0;
}

void ARetrieveEpicMonsterCharacter::StopLocomotionMontages()
{
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
		{
			if (GroundTurnMontage)
			{
				AnimInstance->Montage_Stop(0.12f, GroundTurnMontage);
			}
			if (GroundMoveMontage)
			{
				AnimInstance->Montage_Stop(0.12f, GroundMoveMontage);
			}
		}
	}

	GroundTurnMontage = nullptr;
	GroundMoveMontage = nullptr;
	GroundTurnDirection = 0;
}
