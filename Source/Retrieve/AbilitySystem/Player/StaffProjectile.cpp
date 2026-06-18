#include "AbilitySystem/Player/StaffProjectile.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayEffect.h"
#include "GameplayTags/RetrieveGameplayTags.h"

AStaffProjectile::AStaffProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	InitialLifeSpan = 5.f;

	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->InitSphereRadius(24.f);
	CollisionSphere->SetCollisionObjectType(ECC_WorldDynamic);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionSphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionSphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	CollisionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CollisionSphere->SetGenerateOverlapEvents(true);
	RootComponent = CollisionSphere;

	// TODO(하민:) VFX 에셋 확보 전 임시 비주얼.
	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(CollisionSphere);
	MeshComp->SetRelativeScale3D(FVector(0.25f));
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionSphere;
	ProjectileMovement->InitialSpeed = 1800.f;
	ProjectileMovement->MaxSpeed = 1800.f;
	ProjectileMovement->ProjectileGravityScale = 0.f;
	ProjectileMovement->bRotationFollowsVelocity = true;
}

void AStaffProjectile::Launch(const FVector& Direction, float Speed)
{
	if (!ProjectileMovement)
	{
		return;
	}

	ProjectileMovement->InitialSpeed = Speed;
	ProjectileMovement->MaxSpeed = Speed;
	ProjectileMovement->Velocity = Direction.GetSafeNormal() * Speed;
}

void AStaffProjectile::ConfigureAttack(
	UAbilitySystemComponent* InSourceASC,
	AActor* InInstigatorActor,
	float InDamageMultiplier,
	ERetrieveHitReactType InHitReactType,
	const FGameplayTag& InAttackTypeTag,
	const FGameplayTag& InElementTag,
	TSubclassOf<UGameplayEffect> InElementStatusEffect,
	const FGameplayTag& InChargeBonusEventTag)
{
	SourceASC = InSourceASC;
	InstigatorActor = InInstigatorActor;
	DamageMultiplier = InDamageMultiplier;
	HitReactType = InHitReactType;
	AttackTypeTag = InAttackTypeTag;
	ElementTag = InElementTag;
	ElementStatusEffect = InElementStatusEffect;
	ChargeBonusEventTag = InChargeBonusEventTag;
}

void AStaffProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (CollisionSphere)
	{
		CollisionSphere->OnComponentBeginOverlap.AddDynamic(this, &AStaffProjectile::OnProjectileOverlap);
	}

	if (ProjectileMovement)
	{
		ProjectileMovement->OnProjectileStop.AddDynamic(this, &AStaffProjectile::OnProjectileStopped);
	}
}

void AStaffProjectile::OnProjectileOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (bConsumed || !OtherActor || IsIgnoredActor(OtherActor))
	{
		return;
	}

	// TODO(하민): 팀 필터(ERetrieveTeam) 적용 — 현재는 ASC 보유 Pawn이면 타격(SP 가정).
	IAbilitySystemInterface* TargetInterface = Cast<IAbilitySystemInterface>(OtherActor);
	UAbilitySystemComponent* TargetASC = TargetInterface ? TargetInterface->GetAbilitySystemComponent() : nullptr;
	if (!TargetASC)
	{
		return;
	}

	bConsumed = true;

	if (HasAuthority())
	{
		ApplyHitToTarget(OtherActor, TargetASC, SweepResult);
	}

	if (KnockbackStrength > 0.f)
	{
		if (ACharacter* HitCharacter = Cast<ACharacter>(OtherActor))
		{
			const FVector Direction = (OtherActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
			const FVector LaunchVelocity = Direction * KnockbackStrength + FVector(0.f, 0.f, KnockbackUpwardStrength);
			HitCharacter->LaunchCharacter(LaunchVelocity, true, true);
		}
	}

	Destroy();
}

void AStaffProjectile::ApplyHitToTarget(AActor* TargetActor, UAbilitySystemComponent* TargetASC, const FHitResult& SweepResult)
{
	UAbilitySystemComponent* EffectiveSourceASC = ResolveSourceASC();
	if (!EffectiveSourceASC || !DamageEffectClass || !IsValid(TargetActor) || !IsValid(TargetASC))
	{
		return;
	}

	AActor* InstigatorPtr = InstigatorActor.Get() ? InstigatorActor.Get() : GetOwner();

	// 데미지 GE
	FGameplayEffectContextHandle Context = EffectiveSourceASC->MakeEffectContext();
	Context.AddInstigator(InstigatorPtr, this);   // EffectCauser = 투사체(this)
	Context.AddSourceObject(this);
	Context.AddHitResult(SweepResult, /*bReset=*/true);

	FGameplayEffectSpecHandle Spec = EffectiveSourceASC->MakeOutgoingSpec(DamageEffectClass, 1.f, Context);
	if (Spec.IsValid() && Spec.Data.IsValid())
	{
		Spec.Data->SetSetByCallerMagnitude(RetrieveGameplayTags::Data_Damage_Mul, DamageMultiplier);
		
		AddCombatTagsToDamageSpec(
			*Spec.Data.Get(),
			ElementTag,
			AttackTypeTag.IsValid() ? AttackTypeTag : RetrieveGameplayTags::Attack_Type_Normal,
			FGameplayTag(),
			HitReactTypeToTag(HitReactType));

		EffectiveSourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
	}

	// 원소 상태 GE
	if (ElementStatusEffect)
	{
		FGameplayEffectContextHandle ElementContext = EffectiveSourceASC->MakeEffectContext();
		ElementContext.AddInstigator(InstigatorPtr, this);
		ElementContext.AddSourceObject(this);

		FGameplayEffectSpecHandle ElementSpec = EffectiveSourceASC->MakeOutgoingSpec(ElementStatusEffect, 1.f, ElementContext);
		if (ElementSpec.IsValid() && ElementSpec.Data.IsValid())
		{
			EffectiveSourceASC->ApplyGameplayEffectSpecToTarget(*ElementSpec.Data.Get(), TargetASC);
		}
	}

	// 명중 시 게이지 충전
	if (ChargeBonusEventTag.IsValid() && IsValid(InstigatorPtr))
	{
		FGameplayEventData BonusEvent;
		BonusEvent.Instigator = InstigatorPtr;
		BonusEvent.Target = TargetActor;
		BonusEvent.EventTag = ChargeBonusEventTag;
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(InstigatorPtr, ChargeBonusEventTag, BonusEvent);
	}
}

void AStaffProjectile::OnProjectileStopped(const FHitResult& ImpactResult)
{
	Destroy();
}

bool AStaffProjectile::IsIgnoredActor(const AActor* OtherActor) const
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

UAbilitySystemComponent* AStaffProjectile::ResolveSourceASC() const
{
	if (SourceASC.IsValid())
	{
		return SourceASC.Get();
	}
	
	AActor* SourceActor = InstigatorActor.Get() ? InstigatorActor.Get() : GetOwner();
	if (!SourceActor)
	{
		SourceActor = GetInstigator();
	}

	const IAbilitySystemInterface* SourceInterface = Cast<IAbilitySystemInterface>(SourceActor);
	return SourceInterface ? SourceInterface->GetAbilitySystemComponent() : nullptr;
}
