#include "AbilitySystem/Enemy/GAS/GA_EpicShootProjectiles.h"

#include "AbilitySystem/Enemy/EpicEnemyProjectile.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "Character/RetrieveEnemyCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

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
	FaceCachedTarget();

	if (!bUseAerialModeOnActivate || bAerialModeApplied || !CachedAvatarCharacter)
	{
		return;
	}

	if (ARetrieveEnemyCharacter* EnemyCharacter = Cast<ARetrieveEnemyCharacter>(CachedAvatarCharacter))
	{
		if (const UCharacterMovementComponent* MoveComp = EnemyCharacter->GetCharacterMovement())
		{
			if (MoveComp->MovementMode == MOVE_Flying)
			{
				return;
			}
		}

		EnemyCharacter->SetAerialMode(true);
	}

	CachedAvatarCharacter->SetActorLocation(
		CachedAvatarCharacter->GetActorLocation() + FVector(0.f, 0.f, AerialLiftHeight),
		true);
	bAerialModeApplied = true;
}

void UGA_EpicShootProjectiles::OnSpecialAttackEnded()
{
	if (bAerialModeApplied && CachedAvatarCharacter)
	{
		if (ARetrieveEnemyCharacter* EnemyCharacter = Cast<ARetrieveEnemyCharacter>(CachedAvatarCharacter))
		{
			EnemyCharacter->SetAerialMode(false);
		}
	}

	bAerialModeApplied = false;
	CachedAvatarCharacter = nullptr;
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
