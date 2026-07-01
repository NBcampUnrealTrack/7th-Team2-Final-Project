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
#include "GameplayTags/RetrieveGameplayTags.h"
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
	CollisionSphere->SetCanEverAffectNavigation(false);
	RootComponent = CollisionSphere;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(CollisionSphere);
	MeshComp->SetRelativeScale3D(FVector(0.3f));
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComp->SetCanEverAffectNavigation(false);

	FlightVFXComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("FlightVFXComponent"));
	FlightVFXComponent->SetupAttachment(CollisionSphere);
	FlightVFXComponent->SetAutoActivate(false);
	FlightVFXComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FlightVFXComponent->SetCanEverAffectNavigation(false);

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

void AEnemyProjectile::PrepareForDelayedLaunch()
{
	SetLifeSpan(0.f);

	if (CollisionSphere)
	{
		CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}
}

void AEnemyProjectile::ReleaseDelayedLaunch(
	const FVector& Direction,
	float Speed,
	float Lifetime,
	float GravityScale)
{
	if (CollisionSphere)
	{
		CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	if (ProjectileMovement)
	{
		ProjectileMovement->ProjectileGravityScale = GravityScale;
		ProjectileMovement->Activate(true);
	}

	Launch(Direction, Speed);
	SetProjectileLifetime(Lifetime);
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

void AEnemyProjectile::SetDamageMultiplier(float InDamageMultiplier)
{
	DamageMultiplier = FMath::Max(0.f, InDamageMultiplier);
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
	ConfigureNonBlockingComponents();

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

	if (FlightVFXComponent)
	{
		if (FlightVFX)
		{
			FlightVFXComponent->SetAsset(FlightVFX);
		}

		FlightVFXComponent->SetRelativeLocation(FVector::ZeroVector);
		FlightVFXComponent->SetRelativeRotation(FRotator::ZeroRotator);

		if (FlightVFXComponent->GetAsset())
		{
			FlightVFXComponent->Activate(true);
		}
	}
	else if (FlightVFX)
	{
		UNiagaraComponent* AttachedVFXComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
			FlightVFX,
			GetRootComponent(),
			NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset,
			true);
		if (AttachedVFXComp)
		{
			AttachedVFXComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			AttachedVFXComp->SetGenerateOverlapEvents(false);
			AttachedVFXComp->SetCanEverAffectNavigation(false);
		}
	}
}

void AEnemyProjectile::ConfigureNonBlockingComponents()
{
	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents(PrimitiveComponents);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent)
		{
			continue;
		}

		// 투사체와 VFX는 피해 판정용 쿼리만 수행하고 AI 이동 경로/네비메시에는 장애물로 반영하지 않는다.
		PrimitiveComponent->SetCanEverAffectNavigation(false);
		if (PrimitiveComponent != CollisionSphere)
		{
			PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			PrimitiveComponent->SetGenerateOverlapEvents(false);
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

	ApplyDamage(OtherActor);

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
	// on-stop 반경 데미지는 에픽 AoE 투사체 전용. 일반/보스는 직격 데미지만(원본 동작).
	if (ShouldApplyImpactRadiusDamage())
	{
		ApplyDamageInRadius(ImpactLocation);
	}
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

bool AEnemyProjectile::ApplyDamage(AActor* OtherActor)
{
	if (!HasAuthority() || !DamageEffectClass || !OtherActor || IsIgnoredActor(OtherActor) || !ShouldApplyDamageTo(OtherActor))
	{
		return false;
	}

	IAbilitySystemInterface* TargetInterface = Cast<IAbilitySystemInterface>(OtherActor);
	UAbilitySystemComponent* TargetASC = TargetInterface ? TargetInterface->GetAbilitySystemComponent() : nullptr;
	if (!TargetASC)
	{
		return false;
	}

	AActor* SourceActor = GetOwner() ? GetOwner() : GetInstigator();
	IAbilitySystemInterface* SourceInterface = Cast<IAbilitySystemInterface>(SourceActor);
	UAbilitySystemComponent* SourceASC = SourceInterface ? SourceInterface->GetAbilitySystemComponent() : nullptr;
	if (!SourceASC)
	{
		return false;
	}

	FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
	Context.AddInstigator(SourceActor, this);

	const FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(DamageEffectClass, 1.f, Context);
	if (!Spec.IsValid())
	{
		return false;
	}

	Spec.Data->SetSetByCallerMagnitude(
		RetrieveGameplayTags::Data_Damage_Mul,
		FMath::Max(0.f, DamageMultiplier));

	if (const FGameplayTag ReactTag = HitReactTypeToTag(HitReactType); ReactTag.IsValid())
	{
		Spec.Data->AddDynamicAssetTag(ReactTag);
	}
	if (EffectTag.IsValid())
	{
		Spec.Data->AddDynamicAssetTag(EffectTag);
	}

	SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	return true;
}

void AEnemyProjectile::ApplyDamageInRadius(const FVector& Origin)
{
	const float DamageRadius = FMath::Max3(
		StatusEffectRadius,
		LaunchKnockbackRadius,
		CollisionSphere ? CollisionSphere->GetScaledSphereRadius() : 0.f);
	if (!HasAuthority() || !GetWorld() || !DamageEffectClass || DamageRadius <= 0.f)
	{
		return;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(EnemyProjectileDamage), false, this);
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
		FCollisionShape::MakeSphere(DamageRadius),
		QueryParams))
	{
		return;
	}

	TSet<AActor*> DamagedActors;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* HitActor = Overlap.GetActor();
		if (!HitActor || DamagedActors.Contains(HitActor))
		{
			continue;
		}

		DamagedActors.Add(HitActor);
		ApplyDamage(HitActor);
	}
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
		bForceImpactVFXWorldUpRotation ? FRotator::ZeroRotator : Rotation,
		ImpactVFXScale);

	if (VFXComp)
	{
		VFXComp->SetAutoDestroy(true);
		VFXComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		VFXComp->SetGenerateOverlapEvents(false);
		VFXComp->SetCanEverAffectNavigation(false);
		if (AActor* VFXOwner = VFXComp->GetOwner())
		{
			VFXOwner->SetActorEnableCollision(false);
		}

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
