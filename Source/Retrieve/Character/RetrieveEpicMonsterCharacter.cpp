#include "Character/RetrieveEpicMonsterCharacter.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/Enemy/EnemyCombatComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "DrawDebugHelpers.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/Enemy/NormalMonsterHealthBarComponent.h"
#include "Components/Enemy/EnemySuspicionIndicatorComponent.h"
#include "Components/Enemy/EpicMonsterGroggyComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "UObject/UnrealType.h"

namespace
{
	constexpr int32 EpicTurnLeftDirection = -1;
	constexpr int32 EpicTurnRightDirection = 1;

	static TAutoConsoleVariable<int32> CVarRetrieveEpicAlignmentDebug(
		TEXT("Retrieve.Epic.DebugAlignment"),
		0,
		TEXT("1이면 에픽 몬스터 캡슐/메시/거리 기준점을 PIE 뷰포트에 표시한다."));

	float CalculateMovementDirectionDegrees(const APawn* Pawn, const FVector& Velocity2D);

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

	void ApplyMovementAnimProperties(
		UAnimInstance* AnimInstance,
		const APawn* Pawn,
		const FVector& Movement2D,
		const float GroundSpeed,
		const bool bMovingForAnim,
		const bool bInAerialSpecialMode)
	{
		if (!AnimInstance)
		{
			return;
		}

		SetAnimFloatProperty(AnimInstance, TEXT("Speed"), GroundSpeed);
		SetAnimFloatProperty(AnimInstance, TEXT("GroundSpeed"), GroundSpeed);
		SetAnimFloatProperty(AnimInstance, TEXT("Direction"), CalculateMovementDirectionDegrees(Pawn, Movement2D));
		SetAnimBoolProperty(AnimInstance, TEXT("IsMoving"), bMovingForAnim);
		SetAnimBoolProperty(AnimInstance, TEXT("bIsMoving"), bMovingForAnim);
		SetAnimBoolProperty(AnimInstance, TEXT("ShouldMove"), bMovingForAnim);
		SetAnimBoolProperty(AnimInstance, TEXT("bShouldMove"), bMovingForAnim);
		SetAnimBoolProperty(AnimInstance, TEXT("IsChasing"), bMovingForAnim);
		SetAnimBoolProperty(AnimInstance, TEXT("bIsChasing"), bMovingForAnim);
		SetAnimBoolProperty(AnimInstance, TEXT("IsFlying"), bInAerialSpecialMode);
		SetAnimBoolProperty(AnimInstance, TEXT("bIsFlying"), bInAerialSpecialMode);
		SetAnimBoolProperty(AnimInstance, TEXT("IsAerial"), bInAerialSpecialMode);
		SetAnimBoolProperty(AnimInstance, TEXT("bIsAerial"), bInAerialSpecialMode);
		SetAnimBoolProperty(AnimInstance, TEXT("IsInAir"), bInAerialSpecialMode);
		SetAnimBoolProperty(AnimInstance, TEXT("bIsInAir"), bInAerialSpecialMode);
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

	UAnimSequenceBase* LoadEpicFallbackMoveAnimation(const FName MonsterDataRowName)
	{
		static const FName TreantEpicRowName{TEXT("Treant_Epic")};
		static const FName MagmaEpicRowName{TEXT("Magma_Epic")};
		static TSoftObjectPtr<UAnimSequenceBase> TreantMoveAnimation{
			FSoftObjectPath(TEXT("/Game/External/PolygonalCreaturesPack/PolygonalTreant/Animations/Polygonal_Treant_AnimWalk_Forward_WO_Root.Polygonal_Treant_AnimWalk_Forward_WO_Root"))};
		static TSoftObjectPtr<UAnimSequenceBase> MagmaMoveAnimation{
			FSoftObjectPath(TEXT("/Game/External/PolygonalCreaturesPack/PolygonalMagma/Animations/Demo_Polygonal_Magma_AnimationsMove_Forward_WO_Root.Demo_Polygonal_Magma_AnimationsMove_Forward_WO_Root"))};

		if (MonsterDataRowName == TreantEpicRowName)
		{
			return TreantMoveAnimation.LoadSynchronous();
		}

		if (MonsterDataRowName == MagmaEpicRowName)
		{
			return MagmaMoveAnimation.LoadSynchronous();
		}

		return nullptr;
	}

	void DrawEpicAlignmentDebug(ARetrieveEpicMonsterCharacter* Enemy)
	{
		if (!Enemy || CVarRetrieveEpicAlignmentDebug.GetValueOnGameThread() <= 0)
		{
			return;
		}

		UWorld* World = Enemy->GetWorld();
		UCapsuleComponent* Capsule = Enemy->GetCapsuleComponent();
		USkeletalMeshComponent* Mesh = Enemy->GetMesh();
		if (!World || !Capsule || !Mesh)
		{
			return;
		}

		const FVector ActorLocation = Enemy->GetActorLocation();
		const float CapsuleRadiusValue = Capsule->GetScaledCapsuleRadius();
		const float CapsuleHalfHeightValue = Capsule->GetScaledCapsuleHalfHeight();
		const FVector CapsuleBottom = ActorLocation - FVector(0.f, 0.f, CapsuleHalfHeightValue);
		const FVector MeshLocation = Mesh->GetComponentLocation();
		const FBoxSphereBounds MeshBounds = Mesh->Bounds;

		DrawDebugCapsule(
			World,
			ActorLocation,
			CapsuleHalfHeightValue,
			CapsuleRadiusValue,
			Enemy->GetActorQuat(),
			FColor::Cyan,
			false,
			0.f,
			0,
			2.f);
		DrawDebugSphere(World, ActorLocation, 18.f, 12, FColor::Yellow, false, 0.f, 0, 2.f);
		DrawDebugSphere(World, CapsuleBottom, 16.f, 12, FColor::Green, false, 0.f, 0, 2.f);
		DrawDebugSphere(World, MeshLocation, 14.f, 12, FColor::Magenta, false, 0.f, 0, 2.f);
		DrawDebugBox(World, MeshBounds.Origin, MeshBounds.BoxExtent, FColor::Purple, false, 0.f, 0, 1.f);
		DrawDebugLine(World, ActorLocation, MeshLocation, FColor::Magenta, false, 0.f, 0, 1.f);

		const FMonsterDataRow* Row = Enemy->GetMonsterDataRow();
		FString DebugText = FString::Printf(
			TEXT("%s\nActorZ %.1f | CapsuleBottomZ %.1f | MeshZ %.1f | MeshOriginZDelta %.1f\nRadius %.1f | HalfHeight %.1f | AttackableRange %.1f"),
			*Enemy->GetName(),
			ActorLocation.Z,
			CapsuleBottom.Z,
			MeshLocation.Z,
			MeshLocation.Z - ActorLocation.Z,
			CapsuleRadiusValue,
			CapsuleHalfHeightValue,
			Row ? Row->AttackableRange : -1.f);

		if (const UEnemyCombatComponent* Combat = Enemy->FindComponentByClass<UEnemyCombatComponent>())
		{
			if (AActor* Target = Combat->GetFocusTarget())
			{
				const float Distance3D = FVector::Distance(ActorLocation, Target->GetActorLocation());
				const float Distance2D = FVector::Dist2D(ActorLocation, Target->GetActorLocation());
				DebugText += FString::Printf(
					TEXT("\nTarget %s | Dist3D %.1f | Dist2D %.1f | DeltaZ %.1f"),
					*GetNameSafe(Target),
					Distance3D,
					Distance2D,
					FMath::Abs(ActorLocation.Z - Target->GetActorLocation().Z));
				DrawDebugLine(World, ActorLocation, Target->GetActorLocation(), FColor::Orange, false, 0.f, 0, 2.f);
			}
		}

		DrawDebugString(
			World,
			ActorLocation + FVector(0.f, 0.f, CapsuleHalfHeightValue + 80.f),
			DebugText,
			nullptr,
			FColor::White,
			0.f,
			true);
	}
}

ARetrieveEpicMonsterCharacter::ARetrieveEpicMonsterCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	EpicGroggyComponent = CreateDefaultSubobject<UEpicMonsterGroggyComponent>(TEXT("EpicMonsterGroggyComponent"));

	// 부모(Normal) 기본 크기(64x64)의 1.5배로 확대. 위치도 더 높이 올려 큰 몸체를 벗어나게 한다.
	if (SuspicionIndicatorComponent)
	{
		SuspicionIndicatorComponent->SetDrawSize(FVector2D(96.f, 96.f));
		SuspicionIndicatorComponent->SetRelativeLocation(FVector(0.f, 0.f, 240.f));
	}
}

void ARetrieveEpicMonsterCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	DrawEpicAlignmentDebug(this);

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		const FVector ActorLocation = GetActorLocation();
		FVector Movement2D(MoveComp->Velocity.X, MoveComp->Velocity.Y, 0.f);
		bool bUsingLocationDeltaVelocity = false;
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
				bUsingLocationDeltaVelocity = true;
			}
		}
		LastAnimMovementSampleLocation = ActorLocation;
		bHasAnimMovementSampleLocation = true;

		const float GroundSpeed = Movement2D.Size();
		const bool bInAerialSpecialMode = MoveComp->MovementMode == MOVE_Flying;
		const bool bMovingForAnim = GroundSpeed > AnimMovingSpeedThreshold
			&& !bInAerialSpecialMode
			&& !IsDeadForAnim();
		if (bUsingLocationDeltaVelocity && bMovingForAnim)
		{
			MoveComp->Velocity.X = Movement2D.X;
			MoveComp->Velocity.Y = Movement2D.Y;
		}
		const bool bMovementShouldDriveChase = bMovingForAnim
			&& !bInAerialSpecialMode
			&& !IsAttackingForAnim()
			&& !IsSpecialAttackingForAnim()
			&& !IsHitForAnim()
			&& !IsStaggeredForAnim()
			&& !IsGroggyForAnim()
			&& !IsDeadForAnim();
		SetMovementDrivenChaseTag(OwnedASC.Get(), bMovementDrivenChaseTagAdded, bMovementShouldDriveChase);

		if (bInAerialSpecialMode && (GroundTurnMontage || GroundMoveMontage))
		{
			StopLocomotionMontages();
		}

		if (USkeletalMeshComponent* MeshComp = GetMesh())
		{
			if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
			{
				ApplyMovementAnimProperties(AnimInstance, this, Movement2D, GroundSpeed, bMovingForAnim, bInAerialSpecialMode);
			}
		}

		if (bMovementShouldDriveChase)
		{
			if (GroundTurnMontage)
			{
				StopLocomotionMontages();
			}

			if (!ForcedGroundMoveAnimation.IsNull()
				|| MonsterDataRowName == FName(TEXT("Treant_Epic"))
				|| MonsterDataRowName == FName(TEXT("Magma_Epic")))
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
							if (!MoveSequence)
							{
								MoveSequence = LoadEpicFallbackMoveAnimation(MonsterDataRowName);
							}
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

void ARetrieveEpicMonsterCharacter::ResetRespawnState()
{
	Super::ResetRespawnState();

	// 에픽 그로기(강공격 누적/타이머/쿨다운) 내부 상태 초기화
	if (EpicGroggyComponent)
	{
		EpicGroggyComponent->ResetRespawnState();
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
