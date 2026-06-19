#include "AbilitySystem/Enemy/EpicEnemyProjectile.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Animation/AnimInstance.h"
#include "Character/RetrieveEnemyCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/Enemy/EpicMonsterGroggyComponent.h"
#include "Components/Enemy/PatternCounterComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"

namespace
{
	bool SnapEnemyToGround(ARetrieveEnemyCharacter* Enemy)
	{
		if (!IsValid(Enemy) || !Enemy->GetWorld())
		{
			return false;
		}

		const FVector ActorLocation = Enemy->GetActorLocation();
		const FVector TraceStart = ActorLocation + FVector(0.f, 0.f, 150.f);
		const FVector TraceEnd = ActorLocation - FVector(0.f, 0.f, 5000.f);

		FHitResult Hit;
		FCollisionQueryParams Params(FName(TEXT("ReflectedProjectileGroggyGroundTrace")), false, Enemy);
		if (!Enemy->GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_WorldStatic, Params))
		{
			return false;
		}

		const float HalfHeight = Enemy->GetCapsuleComponent()
			? Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()
			: 0.f;
		Enemy->SetActorLocation(Hit.ImpactPoint + FVector(0.f, 0.f, HalfHeight), false);
		return true;
	}

	void ForceEnemyToGroundForGroggy(AActor* Target)
	{
		ARetrieveEnemyCharacter* Enemy = Cast<ARetrieveEnemyCharacter>(Target);
		if (!Enemy)
		{
			return;
		}

		Enemy->SetAerialMode(false);
		SnapEnemyToGround(Enemy);

		if (UCharacterMovementComponent* MoveComp = Enemy->GetCharacterMovement())
		{
			MoveComp->StopMovementImmediately();
			MoveComp->SetMovementMode(MOVE_Walking);
		}

		if (USkeletalMeshComponent* MeshComp = Enemy->GetMesh())
		{
			if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
			{
				AnimInstance->StopSlotAnimation(0.1f, TEXT("DefaultSlot"));
			}
		}
	}
}

void AEpicEnemyProjectile::ConfigureReflection(AActor* InCounterTarget, float InReflectedSpeedMultiplier)
{
	ReflectedCounterTarget = InCounterTarget;
	bReflectable = IsValid(InCounterTarget);
	ReflectedSpeedMultiplier = FMath::Max(0.1f, InReflectedSpeedMultiplier);
}

bool AEpicEnemyProjectile::HandleReflectedOverlap(AActor* OtherActor, const FHitResult& SweepResult)
{
	if (!bReflected)
	{
		return false;
	}

	if (TryApplyReflectedCounter(OtherActor))
	{
		const FVector ImpactLocation = SweepResult.bBlockingHit
			? FVector(SweepResult.ImpactPoint)
			: GetActorLocation();
		const FRotator ImpactRotation = SweepResult.bBlockingHit
			? SweepResult.ImpactNormal.Rotation()
			: GetActorRotation();
		PlayImpactVFX(ImpactLocation, ImpactRotation);
		Destroy();
	}

	// 반사 상태에서는 베이스의 데미지 처리를 타지 않는다.
	return true;
}

bool AEpicEnemyProjectile::TryReflectOnHit(AActor* OtherActor, UAbilitySystemComponent* OtherASC)
{
	if (!bReflectable || bReflected || !HasAuthority() || !IsValid(OtherActor) || !IsValid(OtherASC))
	{
		return false;
	}

	if (!OtherASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Parrying))
	{
		return false;
	}

	FGameplayEventData ParryEvent;
	ParryEvent.EventTag = RetrieveGameplayTags::GameplayEvent_Parry_Success;
	ParryEvent.Instigator = OtherActor;
	ParryEvent.Target = OtherActor;
	ParryEvent.OptionalObject = ReflectedCounterTarget.Get();
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		OtherActor, RetrieveGameplayTags::GameplayEvent_Parry_Success, ParryEvent);

	ReflectTowardCounterTarget(OtherActor);
	return true;
}

bool AEpicEnemyProjectile::IsIgnoredActor(const AActor* OtherActor) const
{
	if (bReflected)
	{
		// 반사 후에는 발사한 몬스터(Owner/Instigator)도 피격 대상이 된다.
		if (OtherActor == this)
		{
			return true;
		}
		if (const APawn* InstigatorPawn = GetInstigator())
		{
			if (OtherActor == InstigatorPawn->GetController())
			{
				return true;
			}
		}
		return false;
	}

	return Super::IsIgnoredActor(OtherActor);
}

void AEpicEnemyProjectile::ReflectTowardCounterTarget(AActor* ParryingActor)
{
	AActor* CounterTarget = ReflectedCounterTarget.Get();
	if (!ProjectileMovement || !IsValid(CounterTarget))
	{
		Destroy();
		return;
	}

	ReflectedInstigator = ParryingActor;
	bReflected = true;
	StopHoming();

	if (CollisionSphere)
	{
		CollisionSphere->IgnoreActorWhenMoving(CounterTarget, false);
	}

	const FVector TargetLocation = CounterTarget->GetActorLocation();
	const FVector Direction = (TargetLocation - GetActorLocation()).GetSafeNormal();
	const float CurrentSpeed = FMath::Max(ProjectileMovement->Velocity.Size(), ProjectileMovement->MaxSpeed);
	const float ReflectedSpeed = FMath::Max(CurrentSpeed * ReflectedSpeedMultiplier, 300.f);
	Launch(Direction, ReflectedSpeed);
	SetActorRotation(Direction.Rotation());
}

bool AEpicEnemyProjectile::TryApplyReflectedCounter(AActor* OtherActor)
{
	AActor* CounterTarget = ReflectedCounterTarget.Get();
	if (!HasAuthority() || !IsValid(CounterTarget) || OtherActor != CounterTarget)
	{
		return false;
	}

	ForceEnemyToGroundForGroggy(CounterTarget);

	UPatternCounterComponent* CounterComponent = CounterTarget->FindComponentByClass<UPatternCounterComponent>();
	if (!CounterComponent)
	{
		if (UEpicMonsterGroggyComponent* GroggyComponent = CounterTarget->FindComponentByClass<UEpicMonsterGroggyComponent>())
		{
			GroggyComponent->ApplyGroggyState(5.f);
		}
		return true;
	}

	CounterComponent->OpenCounterWindow(0.1f);
	CounterComponent->TryCounter(
		RetrieveGameplayTags::Ability_Player_Parry,
		RetrieveGameplayTags::Element_None,
		ReflectedInstigator.Get());

	if (UEpicMonsterGroggyComponent* GroggyComponent = CounterTarget->FindComponentByClass<UEpicMonsterGroggyComponent>())
	{
		GroggyComponent->ApplyGroggyState(5.f);
	}
	return true;
}
