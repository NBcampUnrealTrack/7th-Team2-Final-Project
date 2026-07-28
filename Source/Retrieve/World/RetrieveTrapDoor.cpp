#include "World/RetrieveTrapDoor.h"

#include "Character/RetrieveEnemyCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/World/RetrieveInteractionResponseComponent.h"
#include "GameFramework/Pawn.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/GameplayMessages/RetrieveGameplayMessageTypes.h"

ARetrieveTrapDoor::ARetrieveTrapDoor()
{
	InteractionResponse = CreateDefaultSubobject<URetrieveInteractionResponseComponent>(TEXT("InteractionResponse"));

	RoomVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("RoomVolume"));
	RoomVolume->SetupAttachment(SceneRoot);
	RoomVolume->SetBoxExtent(FVector(400.f, 400.f, 200.f));
	RoomVolume->SetCollisionProfileName(TEXT("Trigger"));
	RoomVolume->SetGenerateOverlapEvents(true);

	MonsterClass = ARetrieveEnemyCharacter::StaticClass();
}

void ARetrieveTrapDoor::BeginPlay()
{
	Super::BeginPlay();

	if (InteractionResponse)
	{
		InteractionResponse->OnApplied.AddUniqueDynamic(this, &ARetrieveTrapDoor::HandleInteracted);
	}

	if (RoomVolume)
	{
		RoomVolume->OnComponentBeginOverlap.AddUniqueDynamic(this, &ARetrieveTrapDoor::OnRoomBeginOverlap);
	}

	UGameplayMessageSubsystem& MsgSubsys = UGameplayMessageSubsystem::Get(this);
	DiedHandle = MsgSubsys.RegisterListener<FMonsterDiedPayload>(
		RetrieveGameplayTags::Channel_Monster_Died, this, &ARetrieveTrapDoor::OnMonsterDied);
}

void ARetrieveTrapDoor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (DiedHandle.IsValid())
	{
		UGameplayMessageSubsystem::Get(this).UnregisterListener(DiedHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void ARetrieveTrapDoor::HandleInteracted(AActor* InteractionInstigator)
{
	// 트랩 무장 중엔 잠김 — 상호작용해도 문이 열리지 않음.
	if (IsLocked())
	{
		OnLockedInteract(InteractionInstigator);
		return;
	}

	ToggleDoor();
}

void ARetrieveTrapDoor::OnRoomBeginOverlap(UPrimitiveComponent* /*OverlappedComp*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	if (bArmed || bCleared)
	{
		return;
	}

	// 플레이어 진입 시 무장.
	if (const APawn* Pawn = Cast<APawn>(OtherActor))
	{
		if (Pawn->IsPlayerControlled())
		{
			ArmTrap();
		}
	}
}

void ARetrieveTrapDoor::ArmTrap()
{
	if (bArmed || bCleared || !HasAuthority())
	{
		return;
	}

	// 진입 순간 구역 내 몬스터 스냅샷.
	TArray<AActor*> Overlapping;
	RoomVolume->GetOverlappingActors(Overlapping);

	LiveMonsters.Reset();
	for (AActor* Actor : Overlapping)
	{
		if (IsMonster(Actor))
		{
			LiveMonsters.Add(Actor);
		}
	}

	// 몬스터가 없으면 트랩 발동 안 함(빈 방에 갇히는 것 방지).
	if (LiveMonsters.Num() == 0)
	{
		return;
	}

	bArmed = true;
	CloseDoor();      // 이미 닫혀 있으면 no-op
	OnTrapArmed();
}

void ARetrieveTrapDoor::OnMonsterDied(FGameplayTag /*Channel*/, const FMonsterDiedPayload& Payload)
{
	if (!bArmed || bCleared)
	{
		return;
	}

	const AActor* Dead = Payload.DeadActor.Get();

	// 죽은 대상 + 무효(GC된) 항목 정리.
	for (auto It = LiveMonsters.CreateIterator(); It; ++It)
	{
		const AActor* Monster = It->Get();
		if (!Monster || Monster == Dead)
		{
			It.RemoveCurrent();
		}
	}

	if (LiveMonsters.Num() == 0)
	{
		bCleared = true;
		OpenDoor();
		OnTrapCleared();
	}
}

bool ARetrieveTrapDoor::IsMonster(const AActor* Actor) const
{
	return Actor && MonsterClass && Actor->IsA(MonsterClass);
}
