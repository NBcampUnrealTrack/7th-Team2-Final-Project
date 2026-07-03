#include "Enemy/SpawnerBase.h"

#include "Character/RetrievePawnData.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Components/Combat/RetrieveHealthComponent.h"
#include "Components/SphereComponent.h"
#include "Character/RetrieveEnemyCharacter.h"
#include "Components/Enemy/BossHPBarComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/RetrieveMessageTypes.h"

ASpawnerBase::ASpawnerBase()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComp"));
	RootComponent = RootComp;

	SpawnSphereComp = CreateDefaultSubobject<USphereComponent>(TEXT("SpawnSphere"));
	SpawnSphereComp->SetSphereRadius(2000.f);
	SpawnSphereComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SpawnSphereComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	SpawnSphereComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SpawnSphereComp->SetupAttachment(RootComponent);

	DespawnSphereComp = CreateDefaultSubobject<USphereComponent>(TEXT("DespawnSphere"));
	DespawnSphereComp->SetSphereRadius(3000.f);
	DespawnSphereComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DespawnSphereComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	DespawnSphereComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	DespawnSphereComp->SetupAttachment(RootComponent);
}

void ASpawnerBase::BeginPlay()
{
	Super::BeginPlay();

	// 스트리밍 재-BeginPlay 시 중복 바인딩 ensure 방지: AddUnique + EndPlay에서 해제
	SpawnSphereComp->OnComponentBeginOverlap.AddUniqueDynamic(
		this, &ASpawnerBase::OnSpawnSphereBeginOverlap);

	DespawnSphereComp->OnComponentEndOverlap.AddUniqueDynamic(
		this, &ASpawnerBase::OnDespawnSphereEndOverlap);

	if (RespawnTimerHandles.Num() != SpawnList.Num())
	{
		RespawnTimerHandles.SetNum(SpawnList.Num());
	}

	TArray<AActor*> OverlappingActors;
	SpawnSphereComp->GetOverlappingActors(OverlappingActors);
	for (AActor* Actor : OverlappingActors)
	{
		if (IsTriggerActor(Actor))
		{
			SpawnAll();
			break;
		}
	}

	if (UWorld* World = GetWorld())
	{
		RestListenerHandle = UGameplayMessageSubsystem::Get(World)
			.RegisterListener<FRetrievePlayerRestedPayload>(
				RetrieveGameplayTags::Channel_Player_Rested,
				[WeakThis = TWeakObjectPtr<ASpawnerBase>(this)]
				(FGameplayTag, const FRetrievePlayerRestedPayload&)
				{
					if (ASpawnerBase* Spawner = WeakThis.Get())
					{
						Spawner->ForceRespawnAllDeadEntries();
					}
				});
	}
}

void ASpawnerBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SpawnSphereComp)
	{
		SpawnSphereComp->OnComponentBeginOverlap.RemoveDynamic(this, &ASpawnerBase::OnSpawnSphereBeginOverlap);
	}
	if (DespawnSphereComp)
	{
		DespawnSphereComp->OnComponentEndOverlap.RemoveDynamic(this, &ASpawnerBase::OnDespawnSphereEndOverlap);
	}

	for (FTimerHandle& Handle : RespawnTimerHandles)
	{
		GetWorld()->GetTimerManager().ClearTimer(Handle);
	}

	if (UWorld* World = GetWorld())
	{
		UGameplayMessageSubsystem::Get(World).UnregisterListener(RestListenerHandle);
	}

	DespawnAll();

	Super::EndPlay(EndPlayReason);
}

// ──────────────────────────────────────────────
//  Spawn / Despawn / Respawn
// ──────────────────────────────────────────────

void ASpawnerBase::SpawnAll()
{
	if (bIsSpawned)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	if (EntryPawns.Num() != SpawnList.Num())
	{
		EntryPawns.SetNum(SpawnList.Num());
	}

	for (int32 i = 0; i < SpawnList.Num(); ++i)
	{
		const FSpawnEntry& Entry = SpawnList[i];
		FVector SpawnLocation;
		if (!Entry.PawnData || !Entry.PawnData->PawnClass || !TryGetSpawnLocation(i, SpawnLocation))
		{
			continue;
		}

		const FTransform SpawnTransform(GetActorRotation(), SpawnLocation);
		APawn* Pawn = EntryPawns[i].Get();

		if (Pawn)
		{
			URetrieveHealthComponent* HealthComp = Pawn->FindComponentByClass<URetrieveHealthComponent>();
			if (HealthComp && HealthComp->IsDeadOrDying())
			{
				// 사망 상태 
				continue;
			}
			// 생존 + 비활성 → 재활성화
			else if (ARetrieveEnemyCharacter* Enemy = Cast<ARetrieveEnemyCharacter>(Pawn))
			{
				Enemy->ActivateEnemy(SpawnTransform, false);
			}
		}
		else
		{
			// 없음 → 신규 스폰
			Pawn = World->SpawnActorDeferred<APawn>(
				Entry.PawnData->PawnClass, SpawnTransform, nullptr, nullptr,
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

			if (Pawn)
			{
				if (ARetrieveEnemyCharacter* Enemy = Cast<ARetrieveEnemyCharacter>(Pawn))
				{
					Enemy->SetRespawnable(Entry.bIsRespawnable);
					Enemy->OnDeathEnded.AddDynamic(this, &ASpawnerBase::OnEnemyDeath);
				}
				Pawn->FinishSpawning(SpawnTransform);
				EntryPawns[i] = Pawn;
			}
		}

		if (Pawn)
		{
			SpawnedPawns.Add(Pawn);
		}
	}

	bIsSpawned = true;

	for (const TWeakObjectPtr<APawn>& WeakPawn : SpawnedPawns)
	{
		if (APawn* Pawn = WeakPawn.Get())
		{
			if (UBossHPBarComponent* BossHPBar = Pawn->FindComponentByClass<UBossHPBarComponent>())
			{
				BossHPBar->Show();
			}
		}
	}
}

void ASpawnerBase::DespawnAll()
{
	for (const TWeakObjectPtr<APawn>& WeakPawn : SpawnedPawns)
	{
		if (APawn* Pawn = WeakPawn.Get())
		{
			if (UBossHPBarComponent* BossHPBar = Pawn->FindComponentByClass<UBossHPBarComponent>())
			{
				BossHPBar->Hide();
			}
		}
	}

	bIsSpawned = false;

	for (TWeakObjectPtr<APawn>& WeakPawn : SpawnedPawns)
	{
		APawn* Pawn = WeakPawn.Get();
		if (!Pawn)
		{
			continue;
		}

		URetrieveHealthComponent* HealthCom = Pawn->FindComponentByClass<URetrieveHealthComponent>();
		if (HealthCom && HealthCom->IsDeadOrDying())
		{
			continue; // 이미 사망 → 건드리지 않음
		}

		// 생존 → 비활성화 (EntryPawns에 참조 유지됨)
		if (ARetrieveEnemyCharacter* Enemy = Cast<ARetrieveEnemyCharacter>(Pawn))
		{
			Enemy->DeactivateEnemy();
		}
	}
	SpawnedPawns.Reset();
}

void ASpawnerBase::TryRespawnEntry(int32 EntryIndex)
{
	APawn* Pawn = EntryPawns[EntryIndex].Get();
	if (!Pawn)
	{
		return;
	}

	URetrieveHealthComponent* HealthComp = Pawn->FindComponentByClass<URetrieveHealthComponent>();
	if (!HealthComp || !HealthComp->IsDeadOrDying())
	{
		return;
	}

	FVector SpawnLocation;
	if (!TryGetSpawnLocation(EntryIndex, SpawnLocation))
	{
		return;
	}

	const FTransform SpawnTransform(GetActorRotation(), SpawnLocation);
	if (ARetrieveEnemyCharacter* Enemy = Cast<ARetrieveEnemyCharacter>(Pawn))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] Enemy Respawn Start!"), *GetName());
		Enemy->ActivateEnemy(SpawnTransform, true);
		SpawnedPawns.Add(Pawn);
		if (bIsSpawned)
		{
			if (UBossHPBarComponent* BossHPBar = Enemy->FindComponentByClass<UBossHPBarComponent>())
			{
				BossHPBar->Show();
			}
		}
	}
}

// ──────────────────────────────────────────────
//  내부: 거리 체크
// ──────────────────────────────────────────────
bool ASpawnerBase::TryGetSpawnLocation(int32 EntryIndex, FVector& OutLocation) const
{
	if (!SpawnList.IsValidIndex(EntryIndex))
	{
		return false;
	}

	if (const AActor* SpawnPoint = SpawnList[EntryIndex].SpawnPoint)
	{
		OutLocation = SpawnPoint->GetActorLocation();
		return true;
	}

	if (bUseSpawnerLocationWhenSpawnPointMissing)
	{
		OutLocation = GetActorLocation();
		return true;
	}

	return false;
}

bool ASpawnerBase::IsTriggerActor(const AActor* OtherActor) const
{
	if (!OtherActor)
	{
		return false;
	}

	if (TriggerActorClass)
	{
		return OtherActor->IsA(TriggerActorClass);
	}

	const APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	return PC && OtherActor == PC->GetPawn();
}

bool ASpawnerBase::IsPlayerInRange() const
{
	if (!DespawnSphereComp)
	{
		return false;
	}

	TArray<AActor*> OverlappingActors;
	DespawnSphereComp->GetOverlappingActors(OverlappingActors);
	for (AActor* Actor : OverlappingActors)
	{
		if (IsTriggerActor(Actor))
		{
			return true;
		}
	}

	return false;
}

void ASpawnerBase::OnSpawnSphereBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!IsTriggerActor(OtherActor))
	{
		return;
	}

	// 플레이어가 다시 범위 안으로 들어왔다면, 대기 중이던 리스폰 타이머는 취소한다
	// (눈앞에서 갑자기 몬스터가 팝인하는 것을 방지).
	if (UWorld* World = GetWorld())
	{
		for (FTimerHandle& Handle : RespawnTimerHandles)
		{
			World->GetTimerManager().ClearTimer(Handle);
		}
	}

	if (bIsSpawned)
	{
		return;
	}

	SpawnAll();
}

void ASpawnerBase::OnDespawnSphereEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!IsTriggerActor(OtherActor))
	{
		return;
	}

	if (bIsSpawned)
	{
		bool bHasAlivePawn = false;
		for (const TWeakObjectPtr<APawn>& WeakPawn : SpawnedPawns)
		{
			APawn* Pawn = WeakPawn.Get();
			if (!Pawn)
			{
				continue;
			}

			URetrieveHealthComponent* HealthComp = Pawn->FindComponentByClass<URetrieveHealthComponent>();
			if (!HealthComp || !HealthComp->IsDeadOrDying())
			{
				bHasAlivePawn = true;
				break;
			}
		}

		if (bHasAlivePawn)
		{
			DespawnAll();
		}
	}

	// 플레이어가 범위를 완전히 벗어남 → 사망한 엔트리들의 리스폰 카운트를 시작한다.
	StartPendingRespawnTimers();
}

void ASpawnerBase::OnEnemyDeath(AActor* Actor)
{
	SpawnedPawns.RemoveAll([Actor](const TWeakObjectPtr<APawn>& W)
		{
			return W.Get() == Actor;
		});

	if (UBossHPBarComponent* BossHPBar = Cast<AActor>(Actor)->FindComponentByClass<UBossHPBarComponent>())
	{
		BossHPBar->Hide();
	}

	if (!bAllowRespawn)
	{
		return;
	}

	// 사망 시점에 이미 플레이어가 범위 밖이라면(예: DoT로 인한 지연 사망) 다음 EndOverlap을
	// 기대할 수 없으므로 즉시 리스폰 카운트를 시작한다. 범위 안이라면 나중에
	// OnDespawnSphereEndOverlap에서 시작한다.
	if (!IsPlayerInRange())
	{
		StartPendingRespawnTimers();
	}
}

void ASpawnerBase::StartPendingRespawnTimers()
{
	if (!bAllowRespawn)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (int32 i = 0; i < EntryPawns.Num(); ++i)
	{
		APawn* Pawn = EntryPawns[i].Get();
		if (!Pawn)
		{
			continue;
		}

		URetrieveHealthComponent* HealthComp = Pawn->FindComponentByClass<URetrieveHealthComponent>();
		if (!HealthComp || !HealthComp->IsDeadOrDying())
		{
			continue;
		}

		if (World->GetTimerManager().IsTimerActive(RespawnTimerHandles[i]))
		{
			continue;
		}

		FTimerDelegate Del;
		Del.BindUObject(this, &ASpawnerBase::TryRespawnEntry, i);
		World->GetTimerManager().SetTimer(RespawnTimerHandles[i], Del, RespawnDelay, false);
	}
}

void ASpawnerBase::ForceRespawnAllDeadEntries()
{
	if (!bAllowRespawn)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (int32 i = 0; i < EntryPawns.Num(); ++i)
	{
		World->GetTimerManager().ClearTimer(RespawnTimerHandles[i]);
		TryRespawnEntry(i);
	}
}
