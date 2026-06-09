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
		}
	}
}

bool UEncirclementSubsystem::RequestAttackToken(AActor* Target, AActor* Requester)
{
	if (!Requester || !Target)
	{
		return false;
	}
	FRing& Ring = FindOrAddRing(Target);
	CompactInvalidAttackTokens(Ring);
	
	if (Ring.AttackTokens.Contains(Requester))
	{
		return true;
	}
	
	if (Ring.AttackTokens.Num() >= MaxAttackTokens)
	{
		return false;
	}
	
	Ring.AttackTokens.Add(Requester);
	return true;
}

bool UEncirclementSubsystem::HasAttackToken(AActor* Target, AActor* Requester) const
{
	if (!Target || !Requester)
	{
		return false;
	}
	
	if (const FRing* Ring = Rings.Find(Target))
	{
		return Ring->AttackTokens.Contains(Requester);
	}
	
	return false;
}

bool UEncirclementSubsystem::CanRequestAttackToken(AActor* Target, AActor* Requester) const
{
	if (!Target || !Requester)
	{
		return false;
	}

	if (const FRing* Ring = Rings.Find(Target))
	{
		int32 ValidTokenCount = 0;
		for (const TWeakObjectPtr<AActor>& Token : Ring->AttackTokens)
		{
			if (Token.Get() == Requester)
			{
				return true;
			}

			if (Token.IsValid())
			{
				++ValidTokenCount;
			}
		}

		return ValidTokenCount < MaxAttackTokens;
	}

	return true;
}

void UEncirclementSubsystem::ReleaseAttackToken(AActor* Target, AActor* Requester)
{
	if (!Target || !Requester)
	{
		return;
	}
	
	if (FRing* Ring = Rings.Find(Target))
	{
		Ring->AttackTokens.Remove(Requester);
		CompactInvalidAttackTokens(*Ring);
	}
}

FVector UEncirclementSubsystem::GetSlotLocation(const AActor* Target, int32 SlotIndex,
	bool bUseOuterRadius, float MinNoise, float MaxNoise) const
{
	if (!Target || SlotIndex < 0 || SlotIndex >= NumSlots)
	{
		return Target ? Target->GetActorLocation() : FVector::ZeroVector;
	}
	
	const float StepAngle = 2.f * PI / NumSlots;
	const float Angle = SlotIndex * StepAngle;
	float TargetRadius = bUseOuterRadius ? OuterRadius : InnerRadius;
	
	if (bUseOuterRadius)
	{
		TargetRadius += FMath::FRandRange(MinNoise, MaxNoise);
	}
	
	const FVector Offset(FMath::Cos(Angle) * TargetRadius, FMath::Sin(Angle) * TargetRadius, 0.f);
	const FVector RawLocation = Target->GetActorLocation() + Offset;
	
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation Projected;
		if (NavSys->ProjectPointToNavigation(RawLocation, Projected, FVector(80.f, 80.f, 1000.f)))
		{
			return Projected.Location;
		}
	}
	
	return RawLocation;
}

int32 UEncirclementSubsystem::GetCurrentSlot(AActor* Target, AActor* Requester) const
{
	if (!Target || !Requester)
	{
		return INDEX_NONE;
	}
	
	const FRing* Ring = Rings.Find(Target);
	if (!Ring)
	{
		return INDEX_NONE;
	}
	
	return Ring->Slots.IndexOfByKey(Requester);
}

int32 UEncirclementSubsystem::ShiftSlotExplicit(AActor* Target, AActor* Requester, int32 TargetSlotIndex)
{
	if (!Target || !Requester || TargetSlotIndex < 0 || TargetSlotIndex >= NumSlots)
	{
		return INDEX_NONE;
	}

	FRing& Ring = FindOrAddRing(Target);
	CompactInvalidAttackTokens(Ring);

	if (Ring.Slots[TargetSlotIndex].IsValid() && Ring.Slots[TargetSlotIndex] != Requester)
	{
		AActor* OccupyingEnemy = Ring.Slots[TargetSlotIndex].Get();

		// Swap with a non-attacking occupier so orbit movement can continue.
		const int32 PastSlot = Ring.Slots.IndexOfByKey(Requester);
		const bool bOccupierHasToken = Ring.AttackTokens.Contains(OccupyingEnemy);

		if (PastSlot != INDEX_NONE && !bOccupierHasToken)
		{
			// Move the requester into the target slot and move the occupier back.
			Ring.Slots[TargetSlotIndex] = Requester;
			Ring.Slots[PastSlot] = OccupyingEnemy;

			// The displaced enemy keeps a valid slot and will update its destination on the next evaluator tick.
			return TargetSlotIndex;
		}

		return INDEX_NONE; // 밀어낼 조건이 안 되면 실패 보고
	}

	// 기존에 점유하던 슬롯이 있다면 말끔히 정리
	const int32 PastSlot = Ring.Slots.IndexOfByKey(Requester);
	if (PastSlot != INDEX_NONE)
	{
		Ring.Slots[PastSlot].Reset();
	}

	// 새로운 슬롯 강제 점유
	Ring.Slots[TargetSlotIndex] = Requester;
	return TargetSlotIndex;
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

void UEncirclementSubsystem::CompactInvalidAttackTokens(FRing& Ring) const
{
	Ring.AttackTokens.RemoveAllSwap(
		[](const TWeakObjectPtr<AActor>& Token)
		{
			return !Token.IsValid();
		});
}

void UEncirclementSubsystem::DrawDebug() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	
	for (const auto& Pair : Rings)
	{
		AActor* Target = Pair.Key.Get();
		if (!Target)
		{
			continue;
		}
		
		const FRing& Ring = Pair.Value;
		const FVector TargetLoc = Target->GetActorLocation();

		/** 플레이어 주변에 안쪽 원 그리기*/
		DrawDebugCircle(World, TargetLoc, InnerRadius, 32, FColor::Yellow, false, -1.f, 0, 1.f, FVector(1, 0, 0), FVector(0, 1, 0), false);
		
		/** 플레이어 주변에 바깥쪽 원 그리기*/
		DrawDebugCircle(World, TargetLoc, OuterRadius, 32, FColor::Orange, false, -1.f, 0, 1.f, FVector(1, 0, 0), FVector(0, 1, 0), false);

		/** 각 슬롯 방향별 선점 상태를 확인하여 스피어 배치 */
		for (int32 i = 0; i < NumSlots; ++i)
		{
			if (Ring.Slots.IsValidIndex(i) && Ring.Slots[i].IsValid())
			{
				AActor* Shifter = Ring.Slots[i].Get();
				
				// 해당 액터가 공격 토큰을 가지고 있는지 여부 판단
				bool bHasToken = Ring.AttackTokens.Contains(Shifter);
				
				// 토큰 보유 여부에 따라 실시간으로 이너/아우터 좌표를 가져와 구체를 그려줌
				FVector SlotLoc = GetSlotLocation(Target, i, !bHasToken /* 토큰 없으면 아우터 위치 */);
				
				DrawDebugSphere(World, SlotLoc, 20.f, 8, bHasToken ? FColor::Red : FColor::Cyan, false, -1.f);
				DrawDebugLine(World, TargetLoc, SlotLoc, bHasToken ? FColor::Red : FColor::Cyan, false, -1.f);
			}
		}
	}
}
