#include "AbilitySystem/Player/BurstProjectile.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "Components/SphereComponent.h"
#include "Components/Player/PlayerBurstComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Logging/RetrieveLogChannels.h"

ABurstProjectile::ABurstProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	// 콜리전 (루트)
	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
	CollisionComp->InitSphereRadius(20.f);
	CollisionComp->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	CollisionComp->SetGenerateOverlapEvents(true);
	RootComponent = CollisionComp;

	// 이동 (직선, 중력 없음)
	MovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("MovementComp"));
	MovementComp->UpdatedComponent = CollisionComp;
	MovementComp->InitialSpeed = 0.f; // Launch 에서 설정
	MovementComp->MaxSpeed = 3000.f;
	// false: 발사 시 SpawnRotation(검기 기울임 Roll 포함)을 속도 정렬이 0으로 덮어쓰지 않도록.
	// 직선 비행이라 방향엔 영향 없음. 호밍/곡선 추가 시 BP 디테일 패널에서 다시 켤 것.
	MovementComp->bRotationFollowsVelocity = false;
	MovementComp->ProjectileGravityScale = 0.f;
}

void ABurstProjectile::Initialize(const FBurstHitInstance& InHitData, AActor* InInstigator)
{
	HitData = InHitData;
	InstigatorActor = InInstigator;

	UE_LOG(LogRetrieveCombat, Log,
		TEXT("[BurstProjectile] Initialize. Instigator=%s"),
		*GetNameSafe(InInstigator));
}

void ABurstProjectile::Launch(const FVector& Direction, float Speed)
{
	if (!MovementComp)
	{
		return;
	}

	const FVector Vel = Direction.GetSafeNormal() * Speed;
	MovementComp->Velocity = Vel;

	UE_LOG(LogRetrieveCombat, Log,
		TEXT("[BurstProjectile] Launch. Dir=%s Speed=%.1f"),
		*Direction.ToCompactString(), Speed);
}

void ABurstProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (LifeTime > 0.f)
	{
		SetLifeSpan(LifeTime);
	}

	if (CollisionComp)
	{
		CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &ABurstProjectile::OnSphereOverlap);
	}
}

void ABurstProjectile::OnSphereOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this)
	{
		return;
	}

	// Instigator 본인 제외
	if (OtherActor == InstigatorActor.Get())
	{
		return;
	}

	// ASC 보유 액터만 (데미지 받을 수 있는 액터)
	const IAbilitySystemInterface* TargetIF = Cast<IAbilitySystemInterface>(OtherActor);
	if (!TargetIF || !TargetIF->GetAbilitySystemComponent())
	{
		return;
	}

	// 중복 적중 방지 (관통 시 같은 적 반복 적중 방지)
	if (AlreadyHitActors.Contains(OtherActor))
	{
		return;
	}
	AlreadyHitActors.Add(OtherActor);

	// Instigator 의 PlayerBurstComponent 에 적중 보고
	AActor* InstigatorPtr = InstigatorActor.Get();
	if (IsValid(InstigatorPtr))
	{
		UPlayerBurstComponent* Combat = InstigatorPtr->FindComponentByClass<UPlayerBurstComponent>();
		if (IsValid(Combat))
		{
			Combat->ReportProjectileHit(OtherActor, HitData, SweepResult);
		}
	}

	UE_LOG(LogRetrieveCombat, Log,
		TEXT("[BurstProjectile] Hit. Target=%s, RemainPenetrate=%s"),
		*GetNameSafe(OtherActor),
		bDestroyOnFirstHit ? TEXT("No") : TEXT("Yes"));

	// TODO (Step 5): HitData.HitVFX, HitData.HitSound 재생 (GameplayCue 또는 직접 스폰)

	if (bDestroyOnFirstHit)
	{
		Destroy();
	}
}
