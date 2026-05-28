#include "Enemy/SpawnerBase.h"

#include "Character/RetrievePawnData.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Components/RetrieveHealthComponent.h"
#include "Components/SphereComponent.h"
#include "Character/RetrieveEnemyCharacter.h"

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
	
	SpawnAll();
	DespawnAll();
	
	if (RespawnTimerHandles.Num() != SpawnList.Num())
	{
		RespawnTimerHandles.SetNum(SpawnList.Num());
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
		if (!Entry.PawnData || !Entry.PawnData->PawnClass || !IsValid(Entry.SpawnPoint))
		{
			continue;
		}
		
		const FTransform SpawnTransform(GetActorRotation(), Entry.SpawnPoint->GetActorLocation());
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
}

void ASpawnerBase::DespawnAll()
{
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
	
	if (!IsPositionHidden(SpawnList[EntryIndex].SpawnPoint->GetActorLocation()))
	{
		// 아직 시야에 보임 → 이 에너미만 재시도
		FTimerDelegate Del;
		Del.BindUObject(this, &ASpawnerBase::TryRespawnEntry, EntryIndex);
		GetWorld()->GetTimerManager().SetTimer(
			RespawnTimerHandles[EntryIndex], Del, 0.5f, false);
		return;
	}

	const FTransform SpawnTransform(GetActorRotation(), SpawnList[EntryIndex].SpawnPoint->GetActorLocation());
	if (ARetrieveEnemyCharacter* Enemy = Cast<ARetrieveEnemyCharacter>(Pawn))
	{
		UE_LOG(LogTemp, Warning, TEXT("[%s] Enemy Respawn Start!"), *GetName());
		Enemy->ActivateEnemy(SpawnTransform, true);
		SpawnedPawns.Add(Pawn);
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

void ASpawnerBase::OnSpawnSphereBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bIsSpawned || !TriggerActorClass || !OtherActor->IsA(TriggerActorClass))
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
	
	if (!bIsSpawned || !TriggerActorClass || !OtherActor->IsA(TriggerActorClass))
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
		return;
	}
	
	for (int32 i = 0; i < EntryPawns.Num(); ++i)
	{
		if (EntryPawns[i].Get() == Actor)
		{
			FTimerDelegate Del;
			Del.BindUObject(this, &ASpawnerBase::TryRespawnEntry, i);
			GetWorld()->GetTimerManager().SetTimer(
				RespawnTimerHandles[i], Del, RespawnDelay, false);
			break;
		}
	}
}
