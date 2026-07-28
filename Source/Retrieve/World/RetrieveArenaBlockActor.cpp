// Fill out your copyright notice in the Description page of Project Settings.

#include "RetrieveArenaBlockActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/Enemy/BossHPBarComponent.h"
#include "Engine/HitResult.h"
#include "GameFramework/Pawn.h"
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
	Barrier->SetVisibility(false);

	EntryTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("EntryTrigger"));
	EntryTrigger->SetupAttachment(Root);
	EntryTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	EntryTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	EntryTrigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	EntryTrigger->SetBoxExtent(FVector(1200.f, 1200.f, 300.f));
}

void ARetrieveArenaBlockActor::BeginPlay()
{
	Super::BeginPlay();

	BarrierMID = Barrier->CreateDynamicMaterialInstance(0);

	Barrier->OnComponentHit.AddUniqueDynamic(this, &ARetrieveArenaBlockActor::OnBarrierHit);
	EntryTrigger->OnComponentBeginOverlap.AddUniqueDynamic(this, &ARetrieveArenaBlockActor::OnEntryTriggerBeginOverlap);

	UnlockArena();

	UGameplayMessageSubsystem& MsgSubsys = UGameplayMessageSubsystem::Get(this);

	SpottedHandle = MsgSubsys.RegisterListener<FEnemyPlayerSpottedPayload>(
		RetrieveGameplayTags::Channel_Enemy_PlayerSpotted,
		this, &ARetrieveArenaBlockActor::OnPlayerSpotted);

	DiedHandle = MsgSubsys.RegisterListener<FMonsterDiedPayload>(
		RetrieveGameplayTags::Channel_Monster_Died,
		this, &ARetrieveArenaBlockActor::OnMonsterDied);

	PlayerDiedHandle = MsgSubsys.RegisterListener<FPlayerDiedPayload>(
		RetrieveGameplayTags::Channel_Player_Died,
		this, &ARetrieveArenaBlockActor::OnPlayerDied);
}

void ARetrieveArenaBlockActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Barrier)
	{
		Barrier->OnComponentHit.RemoveDynamic(this, &ARetrieveArenaBlockActor::OnBarrierHit);
	}

	if (EntryTrigger)
	{
		EntryTrigger->OnComponentBeginOverlap.RemoveDynamic(this, &ARetrieveArenaBlockActor::OnEntryTriggerBeginOverlap);
	}

	UGameplayMessageSubsystem& MsgSubsys = UGameplayMessageSubsystem::Get(this);

	if (SpottedHandle.IsValid())
	{
		MsgSubsys.UnregisterListener(SpottedHandle);
	}
	if (DiedHandle.IsValid())
	{
		MsgSubsys.UnregisterListener(DiedHandle);
	}
	if (PlayerDiedHandle.IsValid())
	{
		MsgSubsys.UnregisterListener(PlayerDiedHandle);
	}

	GetWorldTimerManager().ClearTimer(HideTimerHandle);

	Super::EndPlay(EndPlayReason);
}

void ARetrieveArenaBlockActor::OnPlayerSpotted(FGameplayTag Channel, const FEnemyPlayerSpottedPayload& Payload)
{
	if (bCleared || bIsLocked)
	{
		return;
	}

	// 다른 적이 아닌, 이 아레나의 보스가 인지했을 때만 대기 상태로 전환
	if (!IsArenaBoss(Payload.InstigatorEnemy.Get(), Payload.InstigatorLocation))
	{
		return;
	}

	// 여기서 바로 결계를 걸지 않는다. 보스가 문/벽 너머로 플레이어를 먼저 인지해도,
	// 플레이어가 EntryTrigger(아레나 입구를 지난 지점)를 실제로 통과할 때만 잠근다.
	CachedBoss = Payload.InstigatorEnemy;
	PendingSpottedPlayer = Payload.SpottedActor;
	bWaitingForPlayerEntry = true;

	// 방이 좁거나 시야선이 막혀 플레이어가 EntryTrigger "안"에서야 보스에게 인지되는 방
	// (예: 불의 수호자 방)에서는, 진입 BeginOverlap이 이미 지나가 다시 발생하지 않으므로
	// 결계가 영영 안 걸리고 보스 HP바도 안 뜬다. 인지 시점에 플레이어가 이미 트리거와
	// 겹쳐 있으면 진입을 기다리지 않고 즉시 잠근다(넓은 방의 "인지→진입" 경로는 그대로 유지).
	if (AActor* Spotted = Payload.SpottedActor.Get())
	{
		if (EntryTrigger && EntryTrigger->IsOverlappingActor(Spotted))
		{
			bWaitingForPlayerEntry = false;
			LockArena();
		}
	}
}

void ARetrieveArenaBlockActor::OnEntryTriggerBeginOverlap(
	UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (bCleared || bIsLocked || !bWaitingForPlayerEntry)
	{
		return;
	}

	if (!OtherActor || OtherActor != PendingSpottedPlayer.Get())
	{
		return;
	}

	bWaitingForPlayerEntry = false;
	LockArena();
}

void ARetrieveArenaBlockActor::OnMonsterDied(FGameplayTag Channel, const FMonsterDiedPayload& Payload)
{
	if (!IsArenaBoss(Payload.DeadActor.Get(), Payload.DeathLocation))
	{
		return;
	}

	bCleared = true;
	bWaitingForPlayerEntry = false;

	UnlockArena();
}

void ARetrieveArenaBlockActor::OnPlayerDied(FGameplayTag Channel, const FPlayerDiedPayload& Payload)
{
	// 보스가 이미 처치됐으면 관여하지 않는다.
	if (bCleared)
	{
		return;
	}

	// 플레이어 사망 → 결계 해제 + 재진입 대기 초기화(다시 들어오면 재잠금 = 재도전).
	bWaitingForPlayerEntry = false;
	PendingSpottedPlayer = nullptr;
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

void ARetrieveArenaBlockActor::SetBossHPBarVisible(bool bVisible)
{
	if (AActor* Boss = CachedBoss.Get())
	{
		if (UBossHPBarComponent* BossHPBar = Boss->FindComponentByClass<UBossHPBarComponent>())
		{
			bVisible ? BossHPBar->Show() : BossHPBar->Hide();
		}
	}
}

void ARetrieveArenaBlockActor::LockArena()
{
	if (bIsLocked || bCleared)
	{
		return;
	}
	bIsLocked = true;

	GetWorldTimerManager().ClearTimer(HideTimerHandle);

	Barrier->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Barrier->SetVisibility(true);

	// 애니메이션 시작 시각 기록 (머티리얼 Time 노드가 자체 재생)
	if (BarrierMID && GetWorld())
	{
		BarrierMID->SetScalarParameterValue(TEXT("LockTime"), GetWorld()->GetTimeSeconds());
	}

	OnArenaActivated();
	SetBossHPBarVisible(true);
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
	if (BarrierMID && GetWorld())
	{
		BarrierMID->SetScalarParameterValue(TEXT("UnlockTime"), GetWorld()->GetTimeSeconds());
	}
	OnArenaDeactivated();
	SetBossHPBarVisible(false);

	GetWorldTimerManager().SetTimer(HideTimerHandle, this,
		&ARetrieveArenaBlockActor::HideBarrier, UnlockDuration, false);
}

void ARetrieveArenaBlockActor::HideBarrier()
{
	if (Barrier)
	{
		Barrier->SetVisibility(false);
	}
}

void ARetrieveArenaBlockActor::TriggerHitRipple(const FVector& HitWorldLocation)
{
	if (!BarrierMID || !GetWorld())
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
