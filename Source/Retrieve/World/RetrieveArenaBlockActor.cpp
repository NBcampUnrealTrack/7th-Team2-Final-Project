// Fill out your copyright notice in the Description page of Project Settings.


#include "RetrieveArenaBlockActor.h"

#include "Components/StaticMeshComponent.h"
#include "Messaging/GameplayMessages/RetrieveGameplayMessageTypes.h"
#include "GameplayTags/RetrieveGameplayTags.h"

ARetrieveArenaBlockActor::ARetrieveArenaBlockActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	Barrier = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Barrier"));
	Barrier->SetupAttachment(Root);
	Barrier->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ARetrieveArenaBlockActor::BeginPlay()
{
	Super::BeginPlay();

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
	Barrier->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Barrier->SetVisibility(true);
	// TODO: 결계FX
}

void ARetrieveArenaBlockActor::UnlockArena()
{
	Barrier->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Barrier->SetVisibility(false);
}
