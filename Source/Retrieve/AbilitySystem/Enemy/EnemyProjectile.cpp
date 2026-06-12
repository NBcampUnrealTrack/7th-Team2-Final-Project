#include "AbilitySystem/Enemy/EnemyProjectile.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "TimerManager.h"

AEnemyProjectile::AEnemyProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	InitialLifeSpan = 5.f;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->InitSphereRadius(30.f);
	CollisionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionSphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionSphere->SetGenerateOverlapEvents(true);
	RootComponent = CollisionSphere;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(CollisionSphere);
	MeshComp->SetRelativeScale3D(FVector(0.3f));
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionSphere;
	ProjectileMovement->InitialSpeed = 1200.f;
	ProjectileMovement->MaxSpeed = 1200.f;
	ProjectileMovement->ProjectileGravityScale = 0.2f;
	ProjectileMovement->bRotationFollowsVelocity = true;
}

void AEnemyProjectile::Launch(const FVector& Direction, float Speed)
{
	if (!ProjectileMovement)
	{
		return;
	}
	
	ProjectileMovement->InitialSpeed = Speed;
	ProjectileMovement->MaxSpeed = Speed;
	ProjectileMovement->Velocity = Direction.GetSafeNormal() * Speed;
}

void AEnemyProjectile::ConfigureHoming(AActor* TargetActor, float StartDelay, float Duration, float Strength)
{
	if (!ProjectileMovement || !IsValid(TargetActor) || Strength <= 0.f)
	{
		return;
	}

	HomingTargetActor = TargetActor;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (StartDelay <= 0.f)
	{
		StartHoming(Strength);
	}
	else
	{
		World->GetTimerManager().SetTimer(
			HomingStartTimerHandle,
			FTimerDelegate::CreateUObject(this, &AEnemyProjectile::StartHoming, Strength),
			StartDelay,
			false);
	}

	if (Duration > 0.f)
	{
		World->GetTimerManager().SetTimer(
			HomingStopTimerHandle,
			this,
			&AEnemyProjectile::StopHoming,
			StartDelay + Duration,
			false);
	}
}

void AEnemyProjectile::SetProjectileLifetime(float Lifetime)
{
	if (Lifetime > 0.f)
	{
		SetLifeSpan(Lifetime);
	}
}

void AEnemyProjectile::SetGravityScale(float GravityScale)
{
	ProjectileMovement->ProjectileGravityScale = GravityScale;
}

void AEnemyProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (CollisionSphere)
	{
		CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &AEnemyProjectile::OnProjectileOverlap);
	}
	
	if (ProjectileMovement)
	{
		ProjectileMovement->OnProjectileStop.AddDynamic(
			this, &AEnemyProjectile::OnProjectileStopped);
	}
}

void AEnemyProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HomingStartTimerHandle);
		World->GetTimerManager().ClearTimer(HomingStopTimerHandle);
	}
    
	Super::EndPlay(EndPlayReason);
}

void AEnemyProjectile::OnProjectileOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                                           UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                           bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || IsIgnoredActor(OtherActor))
	{
		return;
	}

	IAbilitySystemInterface* TargetInterface = Cast<IAbilitySystemInterface>(OtherActor);
	UAbilitySystemComponent* TargetASC = TargetInterface ? TargetInterface->GetAbilitySystemComponent() : nullptr;
	if (!TargetASC)
	{
		return;
	}

	if (HasAuthority() && DamageEffectClass)
	{
		AActor* SourceActor = GetOwner() ? GetOwner() : GetInstigator();
		IAbilitySystemInterface* SourceInterface = Cast<IAbilitySystemInterface>(SourceActor);
		UAbilitySystemComponent* SourceASC = SourceInterface ? SourceInterface->GetAbilitySystemComponent() : nullptr;

		if (SourceASC)
		{
			FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
			Context.AddInstigator(SourceActor, this);

			const FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(DamageEffectClass, 1.f, Context);
			if (Spec.IsValid())
			{
				SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
			}
		}
	}

	if (ACharacter* HitCharacter = Cast<ACharacter>(OtherActor))
	{
		const FVector Direction = (OtherActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
		const FVector LaunchVelocity = Direction * KnockbackStrength + FVector(0.f, 0.f, KnockbackUpwardStrength);
		HitCharacter->LaunchCharacter(LaunchVelocity, true, true);
	}

	Destroy();
}

void AEnemyProjectile::OnProjectileStopped(const FHitResult& ImpactResult)
{
	Destroy();
}

bool AEnemyProjectile::IsIgnoredActor(const AActor* OtherActor) const
{
	if (OtherActor == this || OtherActor == GetOwner() || OtherActor == GetInstigator())
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

void AEnemyProjectile::StartHoming(float Strength)
{
	if (!ProjectileMovement || !IsValid(HomingTargetActor))
	{
		return;
	}

	UPrimitiveComponent* TargetComponent =
		Cast<UPrimitiveComponent>(HomingTargetActor->GetRootComponent());

	if (!TargetComponent)
	{
		return;
	}

	ProjectileMovement->HomingTargetComponent = TargetComponent;
	ProjectileMovement->HomingAccelerationMagnitude = Strength;
	ProjectileMovement->bIsHomingProjectile = true;
}

void AEnemyProjectile::StopHoming()
{
	if (!ProjectileMovement)
	{
		return;
	}

	ProjectileMovement->bIsHomingProjectile = false;
	ProjectileMovement->HomingTargetComponent = nullptr;
}
