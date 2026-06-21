#include "AbilitySystem/Enemy/GAS/GA_EpicShootProjectiles.h"

#include "AbilitySystem/Enemy/EpicEnemyProjectile.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Character/RetrieveEnemyCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Logging/RetrieveLogChannels.h"
#include "TimerManager.h"

UGA_EpicShootProjectiles::UGA_EpicShootProjectiles(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AerialMonsterDataRows.Add(FName(TEXT("Dragon_Epic")));
	AerialTakeOffAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/External/PolygonalCreaturesPack/PolygonalDragon/Animations/Polygonal_Dragon_AnimationFly_Up.Polygonal_Dragon_AnimationFly_Up")));
	AerialHoverAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/External/PolygonalCreaturesPack/PolygonalDragon/Animations/Polygonal_Dragon_AnimationFly_Idle.Polygonal_Dragon_AnimationFly_Idle")));
	AerialLandingAnimation = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/External/PolygonalCreaturesPack/PolygonalDragon/Animations/Polygonal_Dragon_AnimationFly_Down.Polygonal_Dragon_AnimationFly_Down")));
}

const UAnimMontage* UGA_EpicShootProjectiles::ResolveMontage(const FGameplayEventData* TriggerEventData) const
{
	if (bAerialModeApplied)
	{
		if (UAnimMontage* TakeOffMontage = CreateAerialAnimationMontage(AerialTakeOffAnimation, 1))
		{
			return TakeOffMontage;
		}
	}

	return Super::ResolveMontage(TriggerEventData);
}

UAnimMontage* UGA_EpicShootProjectiles::ResolveFallbackSequenceMontage() const
{
	UAnimSequenceBase* AttackSequence = FallbackMontageSequence.LoadSynchronous();
	if (!AttackSequence)
	{
		return nullptr;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	USkeletalMeshComponent* Mesh = AvatarActor ? AvatarActor->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return nullptr;
	}

	return UAnimMontage::CreateSlotAnimationAsDynamicMontage(
		AttackSequence,
		TEXT("DefaultSlot"),
		0.1f,
		0.15f,
		1.f,
		1);
}

void UGA_EpicShootProjectiles::OnSpecialAttackActivated()
{
	CachedAvatarCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	bFinishingAfterAerialHold = false;
	bAerialModeApplied = false;
	bAerialHoverAnimationPlaying = false;
	AerialModeStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	FaceCachedTarget();

	ARetrieveEnemyCharacter* Enemy = Cast<ARetrieveEnemyCharacter>(CachedAvatarCharacter);
	if (!ShouldApplyAerialMode(Enemy))
	{
		return;
	}

	Enemy->ResetAerialSpecialPhase();
	Enemy->SetAerialMode(true);
	Enemy->BeginAerialSpecialPhase();

	bAerialModeApplied = true;
	StartAerialLift();
}

void UGA_EpicShootProjectiles::OnSpecialAttackEnded()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AerialLiftTimerHandle);
		World->GetTimerManager().ClearTimer(AerialFinishTimerHandle);
	}

	bAerialLiftInProgress = false;
	ForceAerialLandingIfStillFlying();

	bAerialModeApplied = false;
	bFinishingAfterAerialHold = false;
	bAerialHoverAnimationPlaying = false;
	AerialModeStartTime = 0.f;
	AerialLiftStartTime = 0.f;
}

void UGA_EpicShootProjectiles::OnBeforeProjectileSpawn()
{
	FaceCachedTarget();
}

void UGA_EpicShootProjectiles::OnProjectileSpawned(AEnemyProjectile* Projectile, AActor* AvatarActor)
{
	if (!GetActiveProjectileConfig().bReflectable)
	{
		return;
	}

	if (AEpicEnemyProjectile* EpicProjectile = Cast<AEpicEnemyProjectile>(Projectile))
	{
		EpicProjectile->ConfigureReflection(AvatarActor, GetActiveProjectileConfig().ReflectedSpeedMultiplier);
	}
}

float UGA_EpicShootProjectiles::AdjustProjectileFireDelay(float FireDelay, int32 ProjectileIndex) const
{
	return FireDelay;
}

void UGA_EpicShootProjectiles::FaceCachedTarget() const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	AActor* TargetActor = GetCachedTargetActor();
	if (!AvatarActor || !TargetActor)
	{
		return;
	}

	FVector Direction = TargetActor->GetActorLocation() - AvatarActor->GetActorLocation();
	Direction.Z = 0.f;
	if (Direction.IsNearlyZero())
	{
		return;
	}

	AvatarActor->SetActorRotation(Direction.Rotation());
}

void UGA_EpicShootProjectiles::FinishAbility()
{
	const float RequiredAerialDuration = FMath::Max(MinimumAerialModeDuration, AerialLiftDuration);
	if (bAerialModeApplied && RequiredAerialDuration > 0.f)
	{
		const float RemainingDuration = RequiredAerialDuration - GetAerialModeElapsedTime();
		if (RemainingDuration > KINDA_SMALL_NUMBER)
		{
			if (UWorld* World = GetWorld())
			{
				bFinishingAfterAerialHold = true;
				World->GetTimerManager().SetTimer(
					AerialFinishTimerHandle,
					this,
					&UGA_EpicShootProjectiles::FinishAbility,
					RemainingDuration,
					false);
				return;
			}
		}
	}

	Super::FinishAbility();
}

void UGA_EpicShootProjectiles::OnMontageCompleted()
{
	if (bAerialModeApplied && !bAerialHoverAnimationPlaying)
	{
		PlayAerialHoverAnimation();
	}

	Super::OnMontageCompleted();
}

void UGA_EpicShootProjectiles::OnMontageInterrupted()
{
	if (bAerialModeApplied && !bAerialHoverAnimationPlaying)
	{
		PlayAerialHoverAnimation();
	}

	Super::OnMontageInterrupted();
}

bool UGA_EpicShootProjectiles::ShouldApplyAerialMode(const ARetrieveEnemyCharacter* Enemy) const
{
	if (!bUseAerialModeOnActivate || !Enemy)
	{
		return false;
	}

	if (AerialMonsterDataRows.IsEmpty())
	{
		return false;
	}

	return AerialMonsterDataRows.Contains(Enemy->GetMonsterDataRowName());
}

void UGA_EpicShootProjectiles::StartAerialLift()
{
	if (!CachedAvatarCharacter || AerialLiftHeight <= 0.f)
	{
		return;
	}

	AerialLiftStartLocation = CachedAvatarCharacter->GetActorLocation();
	AerialLiftTargetLocation = AerialLiftStartLocation + FVector(0.f, 0.f, AerialLiftHeight);
	AerialLiftStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	if (AerialLiftDuration <= KINDA_SMALL_NUMBER)
	{
		CachedAvatarCharacter->SetActorLocation(AerialLiftTargetLocation, true);
		return;
	}

	bAerialLiftInProgress = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			AerialLiftTimerHandle,
			this,
			&UGA_EpicShootProjectiles::UpdateAerialLift,
			0.016f,
			true);
	}
}

void UGA_EpicShootProjectiles::UpdateAerialLift()
{
	UWorld* World = GetWorld();
	if (!World || !CachedAvatarCharacter || !bAerialLiftInProgress)
	{
		return;
	}

	const float ElapsedTime = FMath::Max(0.f, World->GetTimeSeconds() - AerialLiftStartTime);
	const float Alpha = AerialLiftDuration > KINDA_SMALL_NUMBER
		? FMath::Clamp(ElapsedTime / AerialLiftDuration, 0.f, 1.f)
		: 1.f;
	const float SmoothAlpha = FMath::InterpEaseInOut(0.f, 1.f, Alpha, 2.f);
	const FVector NewLocation = FMath::Lerp(AerialLiftStartLocation, AerialLiftTargetLocation, SmoothAlpha);

	CachedAvatarCharacter->SetActorLocation(NewLocation, true);

	if (Alpha >= 1.f - KINDA_SMALL_NUMBER)
	{
		CachedAvatarCharacter->SetActorLocation(AerialLiftTargetLocation, true);
		bAerialLiftInProgress = false;
		World->GetTimerManager().ClearTimer(AerialLiftTimerHandle);
	}
}

UAnimMontage* UGA_EpicShootProjectiles::CreateAerialAnimationMontage(
	const TSoftObjectPtr<UAnimSequenceBase>& Animation,
	const int32 LoopCount) const
{
	UAnimSequenceBase* LoadedAnimation = Animation.LoadSynchronous();
	if (!LoadedAnimation)
	{
		return nullptr;
	}

	return UAnimMontage::CreateSlotAnimationAsDynamicMontage(
		LoadedAnimation,
		AerialAnimationSlot.IsNone() ? FName(TEXT("DefaultSlot")) : AerialAnimationSlot,
		0.12f,
		0.15f,
		FMath::Max(0.1f, AerialAnimationPlayRate),
		LoopCount);
}

void UGA_EpicShootProjectiles::PlayAerialHoverAnimation()
{
	if (!CachedAvatarCharacter)
	{
		return;
	}

	USkeletalMeshComponent* Mesh = CachedAvatarCharacter->FindComponentByClass<USkeletalMeshComponent>();
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	UAnimMontage* HoverMontage = CreateAerialAnimationMontage(AerialHoverAnimation, 999);
	if (!AnimInstance || !HoverMontage)
	{
		return;
	}

	AnimInstance->Montage_Play(HoverMontage, FMath::Max(0.1f, AerialAnimationPlayRate));
	bAerialHoverAnimationPlaying = true;
}

void UGA_EpicShootProjectiles::PlayAerialLandingAnimation()
{
	if (!CachedAvatarCharacter)
	{
		return;
	}

	USkeletalMeshComponent* Mesh = CachedAvatarCharacter->FindComponentByClass<USkeletalMeshComponent>();
	UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
	UAnimMontage* LandingMontage = CreateAerialAnimationMontage(AerialLandingAnimation, 1);
	if (!AnimInstance || !LandingMontage)
	{
		return;
	}

	AnimInstance->Montage_Stop(0.08f, nullptr);
	AnimInstance->Montage_Play(LandingMontage, FMath::Max(0.1f, AerialAnimationPlayRate));
}

float UGA_EpicShootProjectiles::GetAerialModeElapsedTime() const
{
	const UWorld* World = GetWorld();
	if (!World || AerialModeStartTime <= 0.f)
	{
		return 0.f;
	}

	return FMath::Max(0.f, World->GetTimeSeconds() - AerialModeStartTime);
}

void UGA_EpicShootProjectiles::ForceAerialLandingIfStillFlying()
{
	ARetrieveEnemyCharacter* Enemy = Cast<ARetrieveEnemyCharacter>(CachedAvatarCharacter);
	UCharacterMovementComponent* MoveComp = Enemy ? Enemy->GetCharacterMovement() : nullptr;
	if (!bAerialModeApplied || !Enemy || !MoveComp || MoveComp->MovementMode != MOVE_Flying)
	{
		CachedAvatarCharacter = nullptr;
		return;
	}

	Enemy->ResetAerialSpecialPhase();
	PlayAerialLandingAnimation();
	Enemy->SetAerialMode(false);
	MoveComp->SetMovementMode(MOVE_Falling);
	MoveComp->Velocity.Z = FMath::Min(MoveComp->Velocity.Z, -300.f);

	UE_LOG(LogRetrieveCombat, Warning,
		TEXT("[GA_EpicShootProjectiles] Forced aerial landing fallback after projectile pattern ended. Avatar=%s"),
		*GetNameSafe(Enemy));

	CachedAvatarCharacter = nullptr;
}
