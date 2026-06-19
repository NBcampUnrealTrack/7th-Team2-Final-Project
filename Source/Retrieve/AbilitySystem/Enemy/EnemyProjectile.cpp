#include "AbilitySystem/Enemy/EnemyProjectile.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Combat/RetrieveKnockbackLibrary.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GenericTeamAgentInterface.h"
#include "GameplayEffect.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
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

	FlightVFXComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("FlightVFXComponent"));
	FlightVFXComponent->SetupAttachment(CollisionSphere);
	FlightVFXComponent->SetAutoActivate(false);

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

void AEnemyProjectile::SetHitReactType(ERetrieveHitReactType InHitReactType)
{
	HitReactType = InHitReactType;
}

void AEnemyProjectile::SetEffectTag(FGameplayTag InEffectTag)
{
	EffectTag = InEffectTag;
}

void AEnemyProjectile::SetLaunchKnockbackConfig(const FMonsterLaunchKnockbackConfig& InLaunchKnockbackConfig)
{
	LaunchKnockbackConfig = InLaunchKnockbackConfig;
}

void AEnemyProjectile::SetStatusEffectClass(TSubclassOf<UGameplayEffect> InStatusEffectClass)
{
	StatusEffectClass = InStatusEffectClass;
}

void AEnemyProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (CollisionSphere)
	{
		if (AActor* OwnerActor = GetOwner())
		{
			CollisionSphere->IgnoreActorWhenMoving(OwnerActor, true);
		}
		if (APawn* InstigatorPawn = GetInstigator())
		{
			CollisionSphere->IgnoreActorWhenMoving(InstigatorPawn, true);
		}
		CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &AEnemyProjectile::OnProjectileOverlap);
	}

	if (ProjectileMovement)
	{
		ProjectileMovement->OnProjectileStop.AddDynamic(
			this, &AEnemyProjectile::OnProjectileStopped);
	}

	if (FlightVFX)
	{
		if (FlightVFXComponent)
		{
			FlightVFXComponent->SetAsset(FlightVFX);
			FlightVFXComponent->SetRelativeLocation(FVector::ZeroVector);
			FlightVFXComponent->SetRelativeRotation(FRotator::ZeroRotator);
			FlightVFXComponent->Activate(true);
		}
		else
		{
			UNiagaraFunctionLibrary::SpawnSystemAttached(
				FlightVFX,
				GetRootComponent(),
				NAME_None,
				FVector::ZeroVector,
				FRotator::ZeroRotator,
				EAttachLocation::KeepRelativeOffset,
				true);
		}
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

	if (HandleReflectedOverlap(OtherActor, SweepResult))
	{
		return;
	}

	IAbilitySystemInterface* TargetInterface = Cast<IAbilitySystemInterface>(OtherActor);
	UAbilitySystemComponent* TargetASC = TargetInterface ? TargetInterface->GetAbilitySystemComponent() : nullptr;
	if (!TargetASC)
	{
		return;
	}

	if (HasAuthority() && TryReflectOnHit(OtherActor, TargetASC))
	{
		return;
	}

	if (HasAuthority() && DamageEffectClass && ShouldApplyDamageTo(OtherActor))
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
				if (const FGameplayTag ReactTag = HitReactTypeToTag(HitReactType); ReactTag.IsValid())
				{
					Spec.Data->AddDynamicAssetTag(ReactTag);
				}
				if (EffectTag.IsValid())
				{
					Spec.Data->AddDynamicAssetTag(EffectTag);
				}

				SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
			}
		}
	}

	ApplyLaunchKnockback(OtherActor, GetActorLocation());
	ApplyStatusEffect(OtherActor);

	const FVector ImpactLocation = SweepResult.bBlockingHit
		? FVector(SweepResult.ImpactPoint)
		: GetActorLocation();
	const FRotator ImpactRotation = SweepResult.bBlockingHit
		? SweepResult.ImpactNormal.Rotation()
		: GetActorRotation();
	PlayImpactVFX(ImpactLocation, ImpactRotation);
	Destroy();
}

void AEnemyProjectile::OnProjectileStopped(const FHitResult& ImpactResult)
{
	const FVector ImpactLocation = ImpactResult.bBlockingHit
		? FVector(ImpactResult.ImpactPoint)
		: GetActorLocation();
	const FRotator ImpactRotation = ImpactResult.bBlockingHit
		? ImpactResult.ImpactNormal.Rotation()
		: GetActorRotation();
	ApplyLaunchKnockbackInRadius(ImpactLocation);
	ApplyStatusEffectInRadius(ImpactLocation);
	PlayImpactVFX(ImpactLocation, ImpactRotation);
	Destroy();
}

bool AEnemyProjectile::IsIgnoredActor(const AActor* OtherActor) const
{
	if (OtherActor == this)
	{
		return true;
	}

	if (OtherActor == GetOwner() || OtherActor == GetInstigator())
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

bool AEnemyProjectile::IsPlayerTarget(const AActor* OtherActor) const
{
	const APawn* OtherPawn = Cast<APawn>(OtherActor);
	if (!OtherPawn)
	{
		return false;
	}

	FGenericTeamId OtherTeamId = FGenericTeamId::NoTeam;
	if (const IGenericTeamAgentInterface* ControllerTeam = Cast<IGenericTeamAgentInterface>(OtherPawn->GetController()))
	{
		OtherTeamId = ControllerTeam->GetGenericTeamId();
	}

	if (OtherTeamId == FGenericTeamId::NoTeam)
	{
		if (const IGenericTeamAgentInterface* PawnTeam = Cast<IGenericTeamAgentInterface>(OtherPawn))
		{
			OtherTeamId = PawnTeam->GetGenericTeamId();
		}
	}

	return OtherTeamId == FGenericTeamId(static_cast<uint8>(ERetrieveTeam::Player));
}

bool AEnemyProjectile::ShouldApplyDamageTo(const AActor* OtherActor) const
{
	const AActor* SourceActor = GetOwner() ? GetOwner() : GetInstigator();
	if (!SourceActor || !OtherActor)
	{
		return false;
	}

	return FGenericTeamId::GetAttitude(SourceActor, OtherActor) == ETeamAttitude::Hostile;
}

void AEnemyProjectile::ApplyLaunchKnockback(AActor* OtherActor, const FVector& Origin)
{
	if (!HasAuthority() || !LaunchKnockbackConfig.bUseLaunchKnockback || !OtherActor || IsIgnoredActor(OtherActor))
	{
		return;
	}

	ACharacter* HitCharacter = Cast<ACharacter>(OtherActor);
	if (!HitCharacter)
	{
		return;
	}

	FRetrieveKnockbackParams KnockbackParams;
	KnockbackParams.Strength = LaunchKnockbackConfig.KnockbackStrength;
	KnockbackParams.UpwardStrength = LaunchKnockbackConfig.KnockbackUpwardStrength;
	URetrieveKnockbackLibrary::ApplyKnockbackFromSource(HitCharacter, Origin, KnockbackParams);
}

void AEnemyProjectile::ApplyLaunchKnockbackInRadius(const FVector& Origin)
{
	if (!HasAuthority() || !GetWorld() || !LaunchKnockbackConfig.bUseLaunchKnockback || LaunchKnockbackRadius <= 0.f)
	{
		return;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyProjectileLaunchKnockback), false, this);
	QueryParams.AddIgnoredActor(this);
	if (AActor* OwnerActor = GetOwner())
	{
		QueryParams.AddIgnoredActor(OwnerActor);
	}
	if (AActor* InstigatorActor = GetInstigator())
	{
		QueryParams.AddIgnoredActor(InstigatorActor);
	}

	TArray<FOverlapResult> Overlaps;
	if (!GetWorld()->OverlapMultiByObjectType(
		Overlaps,
		Origin,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(LaunchKnockbackRadius),
		QueryParams))
	{
		return;
	}

	TSet<ACharacter*> LaunchedCharacters;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		ACharacter* HitCharacter = Cast<ACharacter>(Overlap.GetActor());
		if (!HitCharacter || LaunchedCharacters.Contains(HitCharacter))
		{
			continue;
		}

		LaunchedCharacters.Add(HitCharacter);
		ApplyLaunchKnockback(HitCharacter, Origin);
	}
}

void AEnemyProjectile::ApplyStatusEffect(AActor* OtherActor)
{
	if (!HasAuthority() || !StatusEffectClass || !OtherActor || IsIgnoredActor(OtherActor) || !IsPlayerTarget(OtherActor))
	{
		return;
	}

	IAbilitySystemInterface* TargetInterface = Cast<IAbilitySystemInterface>(OtherActor);
	UAbilitySystemComponent* TargetASC = TargetInterface ? TargetInterface->GetAbilitySystemComponent() : nullptr;
	if (!TargetASC)
	{
		return;
	}

	AActor* SourceActor = GetOwner() ? GetOwner() : GetInstigator();
	IAbilitySystemInterface* SourceInterface = Cast<IAbilitySystemInterface>(SourceActor);
	UAbilitySystemComponent* SourceASC = SourceInterface ? SourceInterface->GetAbilitySystemComponent() : nullptr;
	UAbilitySystemComponent* SpecSourceASC = SourceASC ? SourceASC : TargetASC;
	if (!SpecSourceASC)
	{
		return;
	}

	FGameplayEffectContextHandle Context = SpecSourceASC->MakeEffectContext();
	Context.AddInstigator(SourceActor, this);

	const FGameplayEffectSpecHandle Spec = SpecSourceASC->MakeOutgoingSpec(StatusEffectClass, 1.f, Context);
	if (Spec.IsValid())
	{
		SpecSourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	}
}

void AEnemyProjectile::ApplyStatusEffectInRadius(const FVector& Origin)
{
	if (!HasAuthority() || !GetWorld() || !StatusEffectClass || StatusEffectRadius <= 0.f)
	{
		return;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyProjectileStatusEffect), false, this);
	QueryParams.AddIgnoredActor(this);
	if (AActor* OwnerActor = GetOwner())
	{
		QueryParams.AddIgnoredActor(OwnerActor);
	}
	if (AActor* InstigatorActor = GetInstigator())
	{
		QueryParams.AddIgnoredActor(InstigatorActor);
	}

	TArray<FOverlapResult> Overlaps;
	if (!GetWorld()->OverlapMultiByObjectType(
		Overlaps,
		Origin,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(StatusEffectRadius),
		QueryParams))
	{
		return;
	}

	TSet<AActor*> AppliedActors;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* HitActor = Overlap.GetActor();
		if (!HitActor || AppliedActors.Contains(HitActor))
		{
			continue;
		}

		AppliedActors.Add(HitActor);
		ApplyStatusEffect(HitActor);
	}
}

void AEnemyProjectile::PlayImpactVFX(const FVector& Location, const FRotator& Rotation)
{
	if (!ImpactVFX || !GetWorld())
	{
		return;
	}

	UNiagaraComponent* VFXComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		this,
		ImpactVFX,
		Location,
		bForceImpactVFXWorldUpRotation ? FRotator::ZeroRotator : Rotation);

	if (VFXComp)
	{
		VFXComp->SetAutoDestroy(true);

		// 루핑 NS도 확실히 제거되도록 월드 타이머 매니저로 3초 후 강제 소멸
		// (ProjectileDestroy 이후에도 월드 타이머는 독립적으로 동작)
		TWeakObjectPtr<UNiagaraComponent> WeakComp(VFXComp);
		FTimerHandle CleanupHandle;
		GetWorld()->GetTimerManager().SetTimer(
			CleanupHandle,
			[WeakComp]()
			{
				if (UNiagaraComponent* Comp = WeakComp.Get())
				{
					Comp->DeactivateImmediate();
					if (AActor* Owner = Comp->GetOwner())
					{
						Owner->Destroy();
					}
				}
			},
			3.0f, false);
	}
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
