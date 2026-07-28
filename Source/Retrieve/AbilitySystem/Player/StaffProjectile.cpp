#include "AbilitySystem/Player/StaffProjectile.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Combat/RetrieveKnockbackLibrary.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameplayEffect.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "NiagaraComponent.h"
#include "TimerManager.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	// 조준점 P를 포물선으로 통과하도록 발사 방향을 구한다. (상용 활/투척 조준 표준)
	// 발사각 theta에 대한 2차방정식을 풀어 각을 얻는다. 사거리 밖이면 false.
	bool SolveBallisticArc(const FVector& M, const FVector& P, float v, float g, FVector& OutDirection)
	{
		if (v <= 0.f || g <= 0.f)
		{
			return false;
		}

		// 1. 머즐에서 목표까지 수평거리(dx), 높이차(dz)로 분해
		const FVector Delta = P - M;
		const FVector Horizontal(Delta.X, Delta.Y, 0.f);
		const float dx = Horizontal.Size();
		const float dz = Delta.Z;
		if (dx < KINDA_SMALL_NUMBER)
		{
			return false;
		}

		// 2. tan(theta)에 대한 2차식  a*T^2 - dx*T + (dz+a) = 0  의 계수 a, 판별식
		const float a = (g * dx * dx) / (2.f * v * v);
		const float Discriminant = dx * dx - 4.f * a * (dz + a);
		if (Discriminant < 0.f)
		{
			return false; // 사거리 밖: 이 속도로는 P에 못 닿음
		}

		// 3. 저각 해로 발사각 theta 계산
		const float TanTheta = (dx - FMath::Sqrt(Discriminant)) / (2.f * a);
		const float Theta = FMath::Atan(TanTheta);

		// 4. 수평 성분 + 수직 성분 합쳐 발사 방향
		const FVector Velocity = Horizontal.GetSafeNormal() * (v * FMath::Cos(Theta))
		                       + FVector::UpVector * (v * FMath::Sin(Theta));
		OutDirection = Velocity.GetSafeNormal();
		return !OutDirection.IsNearlyZero();
	}
}

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

	// 비행 트레일 슬롯. 에셋(TrailVFX)이 있으면 BeginPlay에서 재생. 없으면 조용히 무시.
	TrailVFXComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TrailVFXComponent"));
	TrailVFXComponent->SetupAttachment(CollisionSphere);
	TrailVFXComponent->SetAutoActivate(false);
	
	// 사운드 Component, Flight SFX와 ImpactSFX 실행
	FlightSFXComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("FlightSFXComponent"));
	FlightSFXComponent->SetupAttachment(CollisionSphere);
	FlightSFXComponent->SetAutoActivate(false);
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

	// 발사 사운드는 스폰이 아니라 실제 발사 순간(즉발·지연 모두 이 함수를 통과)에 재생.
	if (!LaunchSound.IsNull())
	{
		if (USoundBase* Sound = LaunchSound.LoadSynchronous())
		{
			UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation());
		}
	}
}

void AStaffProjectile::ArmDelayedLaunch(const FVector& Direction, float Speed, float Delay)
{
	// 발사 전까지 Velocity 0으로 두면 PMC가 움직이지 않아 스폰 위치에 떠 있는다(중력 0 가정). Delay 후 저장된 방향으로 발사.
	PendingLaunchDirection = Direction;
	PendingLaunchSpeed = Speed;
	if (ProjectileMovement)
	{
		ProjectileMovement->Velocity = FVector::ZeroVector;
		ProjectileMovement->InitialSpeed = 0.f;
	}

	UWorld* World = GetWorld();
	if (World && Delay > 0.f)
	{
		World->GetTimerManager().SetTimer(LaunchDelayTimerHandle, this, &AStaffProjectile::HandleDelayedLaunch, Delay, false);
	}
	else
	{
		HandleDelayedLaunch();
	}
}

void AStaffProjectile::HandleDelayedLaunch()
{
	Launch(PendingLaunchDirection, PendingLaunchSpeed);
}

void AStaffProjectile::PlayImpactSFX(const FVector& Location)
{
	if (bImpactSFXPlayed || !ImpactSFX)
	{
		return;
	}
	
	bImpactSFXPlayed = true;
	UGameplayStatics::PlaySoundAtLocation(this, ImpactSFX, Location);
}

void AStaffProjectile::IgnoreOtherProjectile(AStaffProjectile* Other)
{
	if (IsValid(Other) && Other != this && CollisionSphere)
	{
		// 이동 스윕에서 형제 투사체를 건너뛴다(블로킹/오버랩 모두 무시).
		CollisionSphere->IgnoreActorWhenMoving(Other, true);
	}
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

AStaffProjectile* AStaffProjectile::SpawnConfigured(UWorld* World, AActor* AvatarActor, UAbilitySystemComponent* SourceASC,
	UMeshComponent* WeaponMesh, AActor* AimTarget, const FRetrieveProjectileSpawnParams& Params)
{
	if (!Params.ProjectileClass || !IsValid(AvatarActor) || !IsValid(World))
	{
		return nullptr;
	}

	// 스폰 위치: 무기 메시 소켓 → 캐릭터 메시 소켓 → 액터+오프셋
	FVector SpawnLocation = AvatarActor->GetActorLocation() + AvatarActor->GetActorRotation().RotateVector(Params.SpawnOffset);
	bool bUsedSocket = false;
	if (!Params.SpawnSocketName.IsNone())
	{
		if (WeaponMesh && WeaponMesh->DoesSocketExist(Params.SpawnSocketName))
		{
			SpawnLocation = WeaponMesh->GetSocketLocation(Params.SpawnSocketName);
			bUsedSocket = true;
		}
		if (!bUsedSocket)
		{
			if (const ACharacter* Char = Cast<ACharacter>(AvatarActor))
			{
				if (USkeletalMeshComponent* CharMesh = Char->GetMesh())
				{
					if (CharMesh->DoesSocketExist(Params.SpawnSocketName))
					{
						SpawnLocation = CharMesh->GetSocketLocation(Params.SpawnSocketName);
						bUsedSocket = true;
					}
				}
			}
		}
		// 다중 오프셋(얼음창 좌/상/우) 모드: 소켓을 앵커로 두고 그 위에 오프셋을 가산.
		if (bUsedSocket && Params.bAddOffsetToSocketBase)
		{
			SpawnLocation += AvatarActor->GetActorRotation().RotateVector(Params.SpawnOffset);
		}
	}

	// 발사 방향: (액터 정면 강제) -> 조준점 탄도해(아크) -> 오토락 타겟 -> 컨트롤 회전 전방
	FVector Direction = AvatarActor->GetActorForwardVector();
	if (Params.bUseActorForward)
	{
		// 캐릭터 정면으로 직진(Water Heavy 얼음창 등).
		Direction = AvatarActor->GetActorForwardVector();
	}
	else if (Params.bHasAimPoint && Params.GravityScaleOverride > 0.f)
	{
		// 조준점 A를 아크로 통과하는 발사각 계산 → 착탄이 크로스헤어와 일치(숄더 시차 자동 해소).
		const float GravityMag = FMath::Abs(World->GetGravityZ()) * Params.GravityScaleOverride;
		FVector BallisticDir;
		if (SolveBallisticArc(SpawnLocation, Params.AimPointLocation, Params.Speed, GravityMag, BallisticDir))
		{
			Direction = BallisticDir;
		}
		else
		{
			// 사거리 밖: A로 직선 조준(도달 못 하고 짧게 떨어짐 = 임계사거리 초과).
			Direction = (Params.AimPointLocation - SpawnLocation).GetSafeNormal();
		}
	}
	else if (Params.bHasAimPoint)
	{
		// 중력 0(직선): 조준점으로 직선.
		Direction = (Params.AimPointLocation - SpawnLocation).GetSafeNormal();
	}
	else if (IsValid(AimTarget))
	{
		FVector AimLocation = AimTarget->GetActorLocation();
		if (const UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(AimTarget->GetRootComponent()))
		{
			AimLocation = RootPrimitive->Bounds.Origin;
		}
		Direction = (AimLocation - SpawnLocation).GetSafeNormal();
	}
	else if (const ACharacter* SourceChar = Cast<ACharacter>(AvatarActor))
	{
		Direction = SourceChar->GetControlRotation().Vector();
	}
	if (Direction.IsNearlyZero())
	{
		Direction = AvatarActor->GetActorForwardVector();
	}

	const FRotator SpawnRotation = Direction.Rotation();
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = AvatarActor;
	SpawnParams.Instigator = Cast<APawn>(AvatarActor);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AStaffProjectile* Projectile = World->SpawnActor<AStaffProjectile>(Params.ProjectileClass, SpawnLocation, SpawnRotation, SpawnParams);
	if (!Projectile)
	{
		return nullptr;
	}

	Projectile->ConfigureAttack(SourceASC, AvatarActor, Params.DamageMultiplier, Params.HitReactType,
		Params.AttackTypeTag, Params.ElementTag, Params.ElementStatusEffect, Params.ChargeBonusEventTag);
	Projectile->LaunchSound = Params.LaunchSound;
	if (Params.LaunchDelay > 0.f)
	{
		// 스폰 후 잠깐 떠 있다 발사(얼음창 맺힘→발사 연출).
		Projectile->ArmDelayedLaunch(Direction, Params.Speed, Params.LaunchDelay);
	}
	else
	{
		Projectile->Launch(Direction, Params.Speed);
	}

	// 낙차 적용: 활 화살만(스태프 강공 등은 0이라 직선 유지).
	// Launch가 MaxSpeed를 발사속도로 고정하므로 반드시 Launch 뒤에 처리한다.
	// MaxSpeed=0(무제한)으로 풀어야 중력이 낙하를 가속한다(고정 시 등속 clamp로 낙차가 뭉개짐).
	if (Params.GravityScaleOverride > 0.f && Projectile->ProjectileMovement)
	{
		Projectile->ProjectileMovement->ProjectileGravityScale = Params.GravityScaleOverride;
		Projectile->ProjectileMovement->MaxSpeed = 0.f;
	}
	return Projectile;
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

	// 트레일 에셋이 지정돼 있으면 붙여서 재생.
	if (TrailVFX && TrailVFXComponent)
	{
		TrailVFXComponent->SetAsset(TrailVFX);
		TrailVFXComponent->Activate(true);
	}
	
	// 발사음 실행
	if (FlightSFXComponent && FlightSFX)
	{
		FlightSFXComponent->SetSound(FlightSFX);
		FlightSFXComponent->Play();
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
			FRetrieveKnockbackParams KnockbackParams;
			KnockbackParams.Strength = KnockbackStrength;
			KnockbackParams.UpwardStrength = KnockbackUpwardStrength;
			URetrieveKnockbackLibrary::ApplyKnockbackFromSource(HitCharacter, GetActorLocation(), KnockbackParams);
		}
	}
	
	const FVector ImpactLocation = SweepResult.bBlockingHit ? FVector(SweepResult.ImpactPoint) : GetActorLocation();
	
	PlayImpactSFX(ImpactLocation);
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
	const FVector ImpactLocation = ImpactResult.bBlockingHit ? FVector(ImpactResult.ImpactPoint) : GetActorLocation();

	PlayImpactSFX(ImpactLocation);
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
