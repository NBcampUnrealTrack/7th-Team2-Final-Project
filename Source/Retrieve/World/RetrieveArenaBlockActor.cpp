// Fill out your copyright notice in the Description page of Project Settings.


#include "RetrieveArenaBlockActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/HitResult.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Messaging/GameplayMessages/RetrieveGameplayMessageTypes.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "TimerManager.h"

ARetrieveArenaBlockActor::ARetrieveArenaBlockActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Barrier = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Barrier"));
	Barrier->SetupAttachment(Root);
	Barrier->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Barrier->SetNotifyRigidBodyCollision(true);
}

void ARetrieveArenaBlockActor::BeginPlay()
{
	Super::BeginPlay();

	// 결계FX 런타임 제어용 MID 생성
	BarrierMID = Barrier->CreateDynamicMaterialInstance(0);

	// 결계에 부딪히는 충돌 → 피격 링 트리거
	Barrier->OnComponentHit.AddDynamic(this, &ARetrieveArenaBlockActor::OnBarrierHit);

	UnlockArena();

	UGameplayMessageSubsystem& MsgSubsys = UGameplayMessageSubsystem::Get(this);

	SpottedHandle = MsgSubsys.RegisterListener<FEnemyPlayerSpottedPayload>(
		RetrieveGameplayTags::Channel_Enemy_PlayerSpotted,
		this, &ARetrieveArenaBlockActor::OnPlayerSpotted);

	DiedHandle = MsgSubsys.RegisterListener<FMonsterDiedPayload>(
		RetrieveGameplayTags::Channel_Monster_Died,
		this, &ARetrieveArenaBlockActor::OnMonsterDied);
}

void ARetrieveArenaBlockActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGameplayMessageSubsystem& MsgSubsys = UGameplayMessageSubsystem::Get(this);

	if (SpottedHandle.IsValid())
	{
		MsgSubsys.UnregisterListener(SpottedHandle);
	}
	if (DiedHandle.IsValid())
	{
		MsgSubsys.UnregisterListener(DiedHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void ARetrieveArenaBlockActor::OnPlayerSpotted(FGameplayTag Channel, const FEnemyPlayerSpottedPayload& Payload)
{
	if (bCleared)
	{
		return;
	}

	// 다른 적이 아닌, 이 아레나의 보스가 인지했을 때만 잠금
	if (IsArenaBoss(Payload.InstigatorEnemy.Get(), Payload.InstigatorLocation))
	{
		LockArena();
	}
}

void ARetrieveArenaBlockActor::OnMonsterDied(FGameplayTag Channel, const FMonsterDiedPayload& Payload)
{
	if (!IsArenaBoss(Payload.DeadActor.Get(), Payload.DeathLocation))
	{
		return;
	}

	bCleared = true;
	UnlockArena();
}

bool ARetrieveArenaBlockActor::IsArenaBoss(const AActor* Actor, const FVector& Location) const
{
	if (!Actor || !BossClass || !Actor->IsA(BossClass))
	{
		return false;
	}

	// 같은 클래스 보스가 다른 구역에도 있을 때를 대비한 거리 필터
	if (ArenaRadius > 0.f
		&& FVector::DistSquared(GetActorLocation(), Location) > FMath::Square(ArenaRadius))
	{
		return false;
	}

	return true;
}

void ARetrieveArenaBlockActor::LockArena()
{
	if (bIsLocked)
	{
		return; // 이미 활성화됨 — 중복 방지
	}
	bIsLocked = true;

	Barrier->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Barrier->SetVisibility(true);

	// 애니메이션 시작 시각 기록 (머티리얼 Time 노드가 자체 재생)
	if (BarrierMID)
	{
		BarrierMID->SetScalarParameterValue(TEXT("LockTime"), GetWorld()->GetTimeSeconds());
	}

	OnArenaActivated();
}

void ARetrieveArenaBlockActor::UnlockArena()
{
	Barrier->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (!bIsLocked)
	{
		Barrier->SetVisibility(false);
		return;
	}

	bIsLocked = false;

	// 소멸 디졸브 시작: 메시는 유지, UnlockDuration 후 숨김
	if (BarrierMID)
	{
		BarrierMID->SetScalarParameterValue(TEXT("UnlockTime"), GetWorld()->GetTimeSeconds());
	}
	OnArenaDeactivated();

	GetWorldTimerManager().SetTimer(HideTimerHandle, this,
		&ARetrieveArenaBlockActor::HideBarrier, UnlockDuration, false);
}

void ARetrieveArenaBlockActor::HideBarrier()
{
	Barrier->SetVisibility(false);
}

void ARetrieveArenaBlockActor::TriggerHitRipple(const FVector& HitWorldLocation)
{
	if (!BarrierMID)
	{
		return;
	}

	// 밀착 시 매 프레임 재시작되어 링이 안 커지는 것을 막기 위한 스로틀
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now - LastRippleTime < RippleCooldown)
	{
		return;
	}
	LastRippleTime = Now;

	// 히트 위치에서 퍼지는 링. 머티리얼이 (Time - HitTime)으로 반지름을 키운다.
	BarrierMID->SetVectorParameterValue(TEXT("HitLocation"),
		FLinearColor(HitWorldLocation.X, HitWorldLocation.Y, HitWorldLocation.Z, 0.f));
	BarrierMID->SetScalarParameterValue(TEXT("HitTime"), Now);

	OnArenaHit(HitWorldLocation);
}

void ARetrieveArenaBlockActor::OnBarrierHit(UPrimitiveComponent* /*HitComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, FVector /*NormalImpulse*/, const FHitResult& Hit)
{
	// 결계 자신/무효 대상은 무시
	if (!OtherActor || OtherActor == this)
	{
		return;
	}

	// 부딪힌 지점에서 링. TriggerHitRipple 내부 쿨다운이 과도한 재생을 막음.
	TriggerHitRipple(Hit.ImpactPoint);
}
