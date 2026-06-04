#include "Enemy/EncirclementSubsystem.h"
#include "NavigationSystem.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"

static TAutoConsoleVariable<int32> CVarEncircleDebug(
	TEXT("Encircle.Debug"), 0,
	TEXT("1이면 포위 슬롯 링을 디버그 드로우한다."));

int32 UEncirclementSubsystem::RequestSlot(AActor* Target, AActor* Requester)
{
	if (!Target || !Requester)
	{
		return INDEX_NONE;
	}
	
	FRing& Ring = FindOrAddRing(Target);

	// 이미 점유 중이면 그 슬롯 유지
	const int32 Existing = Ring.Slots.IndexOfByKey(Requester);
	if (Existing != INDEX_NONE)
	{
		return Existing;
	}
	
	// 요청자의 타깃 기준 방위각
	const FVector ToReq = Requester->GetActorLocation() - Target->GetActorLocation();
	const float ReqAngle = FMath::Atan2(ToReq.Y, ToReq.X);

	// 그 방위에 가장 가까운 '빈' 슬롯
	int32 BestSlot = INDEX_NONE;
	float BestDelta = TNumericLimits<float>::Max();
	const float StepAngle = 2.f * PI / NumSlots;
	for (int32 i = 0; i < NumSlots; ++i)
	{
		if (Ring.Slots[i].IsValid())
		{
			continue;   // 점유됨
		}
		
		const float Delta = FMath::Abs(FMath::FindDeltaAngleRadians(ReqAngle, i * StepAngle));
		if (Delta < BestDelta)
		{
			BestDelta = Delta; BestSlot = i;
		}
	}
	
	if (BestSlot != INDEX_NONE)
	{
		Ring.Slots[BestSlot] = Requester;
	}
	
	return BestSlot;
}

void UEncirclementSubsystem::ReleaseSlot(AActor* Target, AActor* Requester)
{
	if (FRing* Ring = Rings.Find(Target))
	{
		const int32 Idx = Ring->Slots.IndexOfByKey(Requester);
		if (Idx != INDEX_NONE)
		{
			Ring->Slots[Idx].Reset();
			UE_LOG(LogTemp, Log, TEXT("[%s] Releasing slot %d"), *GetName(), Idx);
		}
	}
}

FVector UEncirclementSubsystem::GetSlotLocation(const AActor* Target, int32 SlotIndex) const
{
	if (!Target || SlotIndex < 0 || SlotIndex >= NumSlots)
	{
		return Target ? Target->GetActorLocation() : FVector::ZeroVector;
	}
	
	const float Angle = SlotIndex * (2.f * PI / NumSlots);
	const FVector Offset(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.f);
	const FVector Raw = Target->GetActorLocation() + Offset;
	
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation Projected;
		if (NavSys->ProjectPointToNavigation(Raw, Projected, FVector(80.f, 80.f, 1000.f)))
		{
			return FVector(Projected.Location.X, Projected.Location.Y,
				   Target->GetActorLocation().Z);
		}
	}
	
	return Raw;
}

void UEncirclementSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	
	DrawDebug();
}

bool UEncirclementSubsystem::IsTickable() const
{
	return CVarEncircleDebug.GetValueOnGameThread() > 0;
}

TStatId UEncirclementSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UEncirclementSubsystem, STATGROUP_Tickables);
}

UEncirclementSubsystem::FRing& UEncirclementSubsystem::FindOrAddRing(AActor* Target)
{
	FRing& Ring = Rings.FindOrAdd(Target);
	
	if (Ring.Slots.Num() != NumSlots)
	{
		Ring.Slots.SetNum(NumSlots);   // 빈 슬롯 초기화
	}
	
	return Ring;
}

void UEncirclementSubsystem::DrawDebug() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	auto DrawRing = [&](const AActor* Target, const FRing* Ring)
	{
		if (!Target)
		{
			return;
		}
		for (int32 i = 0; i < NumSlots; ++i)
		{
			const FVector Loc = GetSlotLocation(Target, i);
			const bool bOccupied = Ring && Ring->Slots.IsValidIndex(i) && Ring->Slots[i].IsValid();
			DrawDebugSphere(World, Loc, 20.f, 8, bOccupied ? FColor::Red : FColor::Green, false, -1.f);
		}
		DrawDebugCircle(World, Target->GetActorLocation(), Radius, 32, FColor::Yellow,
			false, -1.f, 0, 1.f, FVector(1, 0, 0), FVector(0, 1, 0), false);
	};

	if (Rings.Num() > 0)
	{
		for (const TPair<TWeakObjectPtr<AActor>, FRing>& Pair : Rings)
		{
			DrawRing(Pair.Key.Get(), &Pair.Value);
		}
	}
	else if (APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0))
	{
		DrawRing(Player, nullptr);   // 활성 링 없으면 플레이어 기준 프리뷰
	}
}
