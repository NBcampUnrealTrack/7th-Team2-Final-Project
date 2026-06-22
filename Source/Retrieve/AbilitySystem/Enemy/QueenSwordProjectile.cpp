

#include "QueenSwordProjectile.h"

#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

AQueenSwordProjectile::AQueenSwordProjectile()
{
	if (ProjectileMovement == nullptr)
	{
		return;
	}

	if (CollisionSphere == nullptr)
	{
		return;
	}

	ProjectileMovement->bAutoActivate = false;
	ProjectileMovement->ProjectileGravityScale = 0.0f;

	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AQueenSwordProjectile::PrepareProjectile(USceneComponent* FollowComponent)
{
	if (IsValid(FollowComponent) == false || bLaunched)
	{
		return;
	}

	if (ProjectileMovement == nullptr)
	{
		return;
	}
	ProjectileMovement->StopMovementImmediately();
	ProjectileMovement->Deactivate();

	SetActorEnableCollision(false);
	SetLifeSpan(0.f);

	AttachToComponent(FollowComponent, FAttachmentTransformRules::KeepWorldTransform);

	bPrepared = true;
}

bool AQueenSwordProjectile::FireAtTarget(AActor* TargetActor, float Speed, float Lifetime, FVector TargetOffset)
{
	if (bPrepared == false || bLaunched || IsValid(TargetActor) == false || Speed <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector AimLocation = TargetActor->GetActorLocation() + TargetOffset;
	const FVector Direction = (AimLocation - GetActorLocation()).GetSafeNormal();

	if (Direction.IsNearlyZero())
	{
		return false;
	}

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetActorRotation(Direction.Rotation());

	SetActorEnableCollision(true);

	if (CollisionSphere)
	{
		CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	if (ProjectileMovement)
	{
		ProjectileMovement->Activate(true);
	}

	SetProjectileLifetime(FMath::Max(0.1f, Lifetime));
	Launch(Direction, FMath::Max(0.f, Speed));

	bPrepared = false;
	bLaunched = true;

	return true;
	
}
