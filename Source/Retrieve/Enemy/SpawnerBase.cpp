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

	SpawnSphereComp->OnComponentBeginOverlap.AddDynamic(
		this, &ASpawnerBase::OnSpawnSphereBeginOverlap);

	DespawnSphereComp->OnComponentEndOverlap.AddDynamic(
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
}

void ASpawnerBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (FTimerHandle& Handle : RespawnTimerHandles)
	{
		GetWorld()->GetTimerManager().ClearTimer(Handle);
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

	if (!IsPositionHidden(SpawnLocation))
	{
		// 아직 시야에 보임 → 이 에너미만 재시도
		FTimerDelegate Del;
		Del.BindUObject(this, &ASpawnerBase::TryRespawnEntry, EntryIndex);
		GetWorld()->GetTimerManager().SetTimer(
			RespawnTimerHandles[EntryIndex], Del, 0.5f, false);
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

/*
void ASpawnerBase::TryRespawn()
{
	UE_LOG(LogTemp, Warning, TEXT("[Spawner] TryRespawn - EntryPawns=%d"), EntryPawns.Num());
	if (!bAllowRespawn)
	{
		return;
	}

	// 사망 엔트리 중 시야에 노출된 위치가 있으면 재시도
	for (int32 i = 0; i < SpawnList.Num(); ++i)
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

		UE_LOG(LogTemp, Warning, TEXT("[Spawner] Entry[%d] - Pawn=%s, IsDead=%d, IsHidden=%d"),
			i, *GetNameSafe(Pawn),
			HealthComp ? HealthComp->IsDeadOrDying() : -1,
			Pawn ? IsPositionHidden(SpawnList[i].SpawnPoint->GetActorLocation()) : -1);

		if (!IsPositionHidden(SpawnList[i].SpawnPoint->GetActorLocation()))
		{
			GetWorld()->GetTimerManager().SetTimer(
				RespawnTimerHandle, this, &ASpawnerBase::TryRespawn, 0.5f, false);
			return;
		}
	}

	// 전체 사망 위치 hidden → 리스폰 실행
	for (int32 i = 0; i < SpawnList.Num(); ++i)
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

		const FTransform SpawnTransform(GetActorRotation(), SpawnList[i].SpawnPoint->GetActorLocation());
		if (ARetrieveEnemyCharacter* Enemy = Cast<ARetrieveEnemyCharacter>(Pawn))
		{
			Enemy->ActivateEnemy(SpawnTransform, true);

			SpawnedPawns.Add(Pawn);
		}
	}

	bIsSpawned = true;
}
*/

// ──────────────────────────────────────────────
//  내부: 거리 체크 / 시야 판정
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

bool ASpawnerBase::IsPositionHidden(const FVector& WorldPos) const
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	APawn* PlayerPawn = PC ? PC->GetPawn() : nullptr;
	if (!PlayerPawn)
	{
		return false;
	}

	// 후방 부인지 체크
	const FVector ToPos = (WorldPos - PlayerPawn->GetActorLocation()).GetSafeNormal();
	if (FVector::DotProduct(PlayerPawn->GetActorForwardVector(), ToPos) < 0.f)
	{
		return true;
	}

	// 전방이더라도 지형·벽 차폐 확인
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(PlayerPawn);

	GetWorld()->LineTraceSingleByChannel(
		Hit,
		PlayerPawn->GetActorLocation() + FVector(0.f, 0.f, 60.f),
		WorldPos,
		ECC_Visibility, Params);

	return Hit.bBlockingHit;
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

void ASpawnerBase::OnSpawnSphereBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bIsSpawned || !IsTriggerActor(OtherActor))
	{
		return;
	}

	SpawnAll();
}

void ASpawnerBase::OnDespawnSphereEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	UE_LOG(LogTemp, Warning, TEXT("[Spawner] DespawnEndOverlap - bIsSpawned=%d, SpawnedNum=%d, bAllowRespawn=%d"),
		bIsSpawned, SpawnedPawns.Num(), bAllowRespawn);

	if (!bIsSpawned || !IsTriggerActor(OtherActor))
	{
		return;
	}

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

void ASpawnerBase::OnEnemyDeath(AActor* Actor)
{
	UE_LOG(LogTemp, Warning, TEXT("[Spawner] OnEnemyDeath - Before Remove: SpawnedPawns=%d"), SpawnedPawns.Num());

	SpawnedPawns.RemoveAll([Actor](const TWeakObjectPtr<APawn>& W)
		{
			return W.Get() == Actor;
		});

	if (!bAllowRespawn)
	{
		if (UBossHPBarComponent* BossHPBar = Cast<AActor>(Actor)->FindComponentByClass<UBossHPBarComponent>())
		{
			BossHPBar->Hide();
		}
		return;
	}

	for (int32 i = 0; i < EntryPawns.Num(); ++i)
	{
		if (EntryPawns[i].Get() == Actor)
		{
			if (UBossHPBarComponent* BossHPBar = Cast<AActor>(Actor)->FindComponentByClass<UBossHPBarComponent>())
			{
				BossHPBar->Hide();
			}

			FTimerDelegate Del;
			Del.BindUObject(this, &ASpawnerBase::TryRespawnEntry, i);
			GetWorld()->GetTimerManager().SetTimer(
				RespawnTimerHandles[i], Del, RespawnDelay, false);
			break;
		}
	}
}
