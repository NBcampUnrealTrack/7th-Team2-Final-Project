#include "Enemy/EncirclementSubsystem.h"
#include "NavigationSystem.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Character/RetrieveEnemyCharacter.h"
#include "Data/RetrieveDataTableTypes.h"

static TAutoConsoleVariable<int32> CVarEncircleDebug(
	TEXT("Encircle.Debug"), 0,
	TEXT("1이면 포위 슬롯 링을 디버그 드로우한다."));

static TAutoConsoleVariable<float> CVarEncircleAnchorDeadband(
	TEXT("Encircle.AnchorDeadband"), 180.f,
	TEXT("대기자 링이 재정렬되기까지 플레이어가 이동해야 하는 거리(언리얼 단위). 값이 작으면 링이 플레이어를 바짝 따라붙고, 크면 플레이어가 확실히 자리를 옮겼을 때만 재정렬됨."));

static TAutoConsoleVariable<float> CVarEncircleMinArc(
	TEXT("Encircle.MinArc"), 200.f,
	TEXT("인접한 대기자 슬롯 간 최소 간격(언리얼 단위). 인원 수에 따른 링 반경 계산에 사용됨. 캡슐 반경의 2배 이상으로 설정할 것."));

static TAutoConsoleVariable<float> CVarEncircleTokenCooldown(
	TEXT("Encircle.TokenCooldown"), 2.0f,
	TEXT("적이 공격 토큰을 반납한 뒤, 다음 공격이 가능해지기까지의 대기 시간(초)."));

int32 UEncirclementSubsystem::RequestSlot(AActor* Target, AActor* Requester)
{
	if (!Target || !Requester)
	{
		return INDEX_NONE;
	}
	FRing& Ring = FindOrAddRing(Target);

	const int32 Existing = Ring.Slots.IndexOfByKey(Requester);
	if (Existing != INDEX_NONE)
	{
		return Existing;
	}

	const FVector ToReq = Requester->GetActorLocation() - Target->GetActorLocation();
	const float ReqAngle = FMath::Atan2(ToReq.Y, ToReq.X);

	const int32 BestSlot = PickBalancedSlot(Ring, ReqAngle);
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
	if (IsOnTokenCooldown(Ring, Requester))
	{
		return false;
	}

	const int32 RequestCost = GetAttackTokenCost(Requester);
	const int32 CurrentCost = GetCurrentAttackTokenCost(Ring);
	const int32 Budget = GetAttackTokenBudget(Ring, Requester);
	if (CurrentCost + RequestCost > Budget)
	{
		return false;
	}
	if (!IsAmongBestCandidates(Ring, Target, Requester, Budget - CurrentCost))
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
	static const FRing EmptyRing;
	const FRing* RingPtr = Rings.Find(Target);
	const FRing& Ring = RingPtr ? *RingPtr : EmptyRing;

	if (Ring.AttackTokens.Contains(Requester))
	{
		return true;
	}
	if (IsOnTokenCooldown(Ring, Requester))
	{
		return false;
	}
	const int32 RequestCost = GetAttackTokenCost(Requester);
	const int32 CurrentCost = GetCurrentAttackTokenCost(Ring);
	const int32 Budget = GetAttackTokenBudget(Ring, Requester);
	if (CurrentCost + RequestCost > Budget)
	{
		return false;
	}
	return IsAmongBestCandidates(Ring, Target, Requester, Budget - CurrentCost);
}

FVector UEncirclementSubsystem::GetOrUpdateRingAnchor(AActor* Target)
{
	if (!Target)
	{
		return FVector::ZeroVector;
	}
	FRing& Ring = FindOrAddRing(Target);
	const FVector PlayerLoc = Target->GetActorLocation();
	const float Deadband = CVarEncircleAnchorDeadband.GetValueOnGameThread();

	// 플레이어가 명확하게 위치를 옮겼을 때만 링을 재배치한다
	if (!Ring.bAnchorValid || FVector::Dist2D(Ring.Anchor, PlayerLoc) > Deadband)
	{
		Ring.Anchor = PlayerLoc;
		Ring.bAnchorValid = true;
	}
	return Ring.Anchor;
}

void UEncirclementSubsystem::ReleaseAttackToken(AActor* Target, AActor* Requester)
{
	if (!Target || !Requester)
	{
		return;
	}
	if (FRing* Ring = Rings.Find(Target))
	{
		const int32 RemovedCount = Ring->AttackTokens.Remove(Requester);
		if (RemovedCount > 0)
		{
			if (const UWorld* World = GetWorld())
			{
				Ring->TokenReleaseTime.Add(Requester, World->GetTimeSeconds());
				// Encircle.Debug 활성화 시 토큰 반환과 쿨다운 시작 시점을 기록한다.
				if (CVarEncircleDebug.GetValueOnGameThread() >= 1)
				{
					UE_LOG(LogTemp, Log, TEXT("[Encircle] ReleaseAttackToken: %s Removed=%d Time=%.3f"),
						*Requester->GetName(), RemovedCount, World->GetTimeSeconds());
				}
			}
		}
		CompactInvalidAttackTokens(*Ring);
	}
}

FVector UEncirclementSubsystem::GetSlotLocation(const AActor* Target, int32 SlotIndex, bool bUseOuterRadius,
                                                float MinNoise, float MaxNoise, float InnerRadiusOverride,
                                                float OuterRadiusOverride, float MaxWaiterRadiusOverride) const
{
	if (!Target || SlotIndex < 0 || SlotIndex >= NumSlots)
	{
		return Target ? Target->GetActorLocation() : FVector::ZeroVector;
	}

	const float StepAngle = 2.f * PI / NumSlots;
	const float Angle = SlotIndex * StepAngle; // 슬롯 각도는 고정하며 이동은 재슬롯으로 표현한다.

	const FRing* Ring = Rings.Find(Target);
	const float BaseInner = InnerRadiusOverride > 0.f ? InnerRadiusOverride : InnerRadius;
	const float BaseOuter = OuterRadiusOverride > 0.f ? OuterRadiusOverride : OuterRadius;

	FVector Center;
	float TargetRadius;
	if (bUseOuterRadius)
	{
		// 대기자: 작은 플레이어 이동이 고리를 끌고 다니지 않도록 고정된 앵커를 기준으로 구성
		Center = (Ring && Ring->bAnchorValid) ? Ring->Anchor : Target->GetActorLocation();
		const int32 NumWaiters = Ring ? FMath::Max(1, CountWaiters(*Ring)) : 1;
		TargetRadius = ComputeRingRadius(NumWaiters, BaseOuter, MaxWaiterRadiusOverride);
		TargetRadius += SlotRadiusNoise(SlotIndex, MinNoise, MaxNoise);
	}
	else
	{
		// 공격자는 링 앵커를 기준으로 Inner 반경을 사용한다.
		Center = (Ring && Ring->bAnchorValid) ? Ring->Anchor : Target->GetActorLocation();
		TargetRadius = BaseInner;
	}

	const FVector Offset(FMath::Cos(Angle) * TargetRadius, FMath::Sin(Angle) * TargetRadius, 0.f);
	const FVector RawLocation = Center + Offset;

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

FVector UEncirclementSubsystem::GetOverflowStandoffLocation(const AActor* Target, const AActor* Requester,
                                                            float DesiredRadius) const
{
	if (!Target || !Requester)
	{
		return Target ? Target->GetActorLocation() : FVector::ZeroVector;
	}
	const FRing* Ring = Rings.Find(Target);
	const FVector Center = (Ring && Ring->bAnchorValid) ? Ring->Anchor : Target->GetActorLocation();

	// 초과된 적들이 한 지점에 쌓이지 않고 흩어지도록 적별로 고정된 각도를 사용
	const uint32 Hash = Requester->GetUniqueID();
	const float Angle = FMath::DegreesToRadians(static_cast<float>(Hash % 360));
	const float Radius = FMath::Max(DesiredRadius, OuterRadius) + 120.f; // 대기자 링보다 살짝 바깥

	const FVector Offset(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 0.f);
	const FVector Raw = Center + Offset;

	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation Projected;
		if (NavSys->ProjectPointToNavigation(Raw, Projected, FVector(120.f, 120.f, 1000.f)))
		{
			return Projected.Location;
		}
	}
	return Raw;
}

int32 UEncirclementSubsystem::GetCommittedCount(const AActor* Target) const
{
	const FRing* Ring = Rings.Find(Target);
	if (!Ring)
	{
		return 0;
	}
	int32 Count = 0;
	for (const TWeakObjectPtr<AActor>& Slot : Ring->Slots)
	{
		if (Slot.IsValid())
		{
			++Count;
		}
	}
	return Count;
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

		// 빈 슬롯이 있으면 교환하지 않고 호출부가 다른 후보를 찾도록 실패를 반환한다.
		const bool bHasEmptySlot = Ring.Slots.ContainsByPredicate(
			[](const TWeakObjectPtr<AActor>& Slot) { return !Slot.IsValid(); });
		if (bHasEmptySlot)
		{
			return INDEX_NONE;
		}

		// 링이 가득 찬 경우 공격 토큰이 없는 점유자와 슬롯을 교환한다.
		const int32 PastSlot = Ring.Slots.IndexOfByKey(Requester);
		const bool bOccupierHasToken = Ring.AttackTokens.Contains(OccupyingEnemy);

		if (PastSlot != INDEX_NONE && !bOccupierHasToken)
		{
			Ring.Slots[TargetSlotIndex] = Requester;
			Ring.Slots[PastSlot] = OccupyingEnemy;

			// 밀려난 점유자는 요청자의 이전 슬롯을 유지한다.
			return TargetSlotIndex;
		}

		return INDEX_NONE;
	}

	const int32 PastSlot = Ring.Slots.IndexOfByKey(Requester);
	if (PastSlot != INDEX_NONE)
	{
		Ring.Slots[PastSlot].Reset();
	}

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
		Ring.Slots.SetNum(NumSlots);
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

int32 UEncirclementSubsystem::GetAttackTokenCost(const AActor* Requester) const
{
	const ARetrieveEnemyCharacter* Enemy = Cast<ARetrieveEnemyCharacter>(Requester);
	const FMonsterDataRow* Row = Enemy ? Enemy->GetMonsterDataRow() : nullptr;
	return Row ? FMath::Max(1, Row->AttackTokenCost) : 1;
}

int32 UEncirclementSubsystem::GetAttackTokenBudget(const FRing& Ring, const AActor* Requester) const
{
	int32 Budget = DefaultAttackTokenBudget;

	auto ApplyBudgetFromActor = [&Budget](const AActor* Actor)
	{
		const ARetrieveEnemyCharacter* Enemy = Cast<ARetrieveEnemyCharacter>(Actor);
		const FMonsterDataRow* Row = Enemy ? Enemy->GetMonsterDataRow() : nullptr;
		if (Row && Row->AttackTokenBudget > 0)
		{
			Budget = FMath::Max(Budget, Row->AttackTokenBudget);
		}
	};

	ApplyBudgetFromActor(Requester);
	for (const TWeakObjectPtr<AActor>& Token : Ring.AttackTokens)
	{
		ApplyBudgetFromActor(Token.Get());
	}

	return FMath::Max(1, Budget);
}

int32 UEncirclementSubsystem::GetCurrentAttackTokenCost(const FRing& Ring) const
{
	int32 Cost = 0;
	for (const TWeakObjectPtr<AActor>& Token : Ring.AttackTokens)
	{
		if (const AActor* TokenActor = Token.Get())
		{
			Cost += GetAttackTokenCost(TokenActor);
		}
	}

	return Cost;
}

bool UEncirclementSubsystem::IsOnTokenCooldown(const FRing& Ring, const AActor* Requester) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	if (const float* Released = Ring.TokenReleaseTime.Find(Requester))
	{
		return (World->GetTimeSeconds() - *Released) < CVarEncircleTokenCooldown.GetValueOnGameThread();
	}
	return false;
}

float UEncirclementSubsystem::ComputeRingRadius(int32 NumOccupants, float BaseRadius, float MaxRadius) const
{
	if (NumOccupants <= 1)
	{
		return BaseRadius;
	}
	const float MinArc = CVarEncircleMinArc.GetValueOnGameThread();
	const float NeededByArc = (NumOccupants * MinArc) / (2.f * PI);
	float R = FMath::Max(BaseRadius, NeededByArc);
	if (MaxRadius > 0.f)
	{
		R = FMath::Min(R, MaxRadius); // 대기자가 StrafeOffRange 내부에 머물도록 함
	}
	return R;
}

int32 UEncirclementSubsystem::CountWaiters(const FRing& Ring) const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AActor>& Slot : Ring.Slots)
	{
		if (Slot.IsValid() && !Ring.AttackTokens.Contains(Slot))
		{
			++Count;
		}
	}
	return Count;
}

float UEncirclementSubsystem::SlotRadiusNoise(int32 SlotIndex, float MinNoise, float MaxNoise) const
{
	if (MaxNoise <= MinNoise)
	{
		return 0.f;
	}
	const float H = FMath::Frac(FMath::Sin(SlotIndex * 12.9898f) * 43758.5453f); // [0,1), 슬롯마다 고정됨
	return FMath::Lerp(MinNoise, MaxNoise, H);
}

bool UEncirclementSubsystem::IsAmongBestCandidates(const FRing& Ring, const AActor* Target, const AActor* Requester,
                                                   int32 SlotsAvailable) const
{
	if (SlotsAvailable <= 0 || !Target || !Requester)
	{
		return false;
	}
	const FVector TargetLoc = Target->GetActorLocation();
	const float ReqDistSq = FVector::DistSquared(Requester->GetActorLocation(), TargetLoc);

	int32 Better = 0;
	for (const TWeakObjectPtr<AActor>& SlotPtr : Ring.Slots)
	{
		const AActor* Other = SlotPtr.Get();
		if (!Other || Other == Requester)
		{
			continue;
		}
		if (Ring.AttackTokens.Contains(Other) || IsOnTokenCooldown(Ring, Other))
		{
			continue;
		}
		if (FVector::DistSquared(Other->GetActorLocation(), TargetLoc) < ReqDistSq)
		{
			++Better;
		}
	}
	return Better < SlotsAvailable;
}

int32 UEncirclementSubsystem::PickBalancedSlot(const FRing& Ring, float BearingAngle) const
{
	const float StepAngle = 2.f * PI / NumSlots;
	int32 BestSlot = INDEX_NONE;
	int32 BestGap = -1;
	float BestBearingDelta = TNumericLimits<float>::Max();

	for (int32 i = 0; i < NumSlots; ++i)
	{
		if (Ring.Slots[i].IsValid())
		{
			continue;
		}
		const int32 Gap = DistanceToNearestOccupied(Ring, i);
		const float BearingDelta = FMath::Abs(FMath::FindDeltaAngleRadians(BearingAngle, i * StepAngle));
		if (Gap > BestGap || (Gap == BestGap && BearingDelta < BestBearingDelta))
		{
			BestGap = Gap;
			BestBearingDelta = BearingDelta;
			BestSlot = i;
		}
	}
	return BestSlot;
}

int32 UEncirclementSubsystem::DistanceToNearestOccupied(const FRing& Ring, int32 SlotIndex) const
{
	int32 Best = NumSlots;
	bool bAnyOccupied = false;
	for (int32 j = 0; j < NumSlots; ++j)
	{
		if (!Ring.Slots[j].IsValid())
		{
			continue;
		}
		bAnyOccupied = true;
		const int32 Raw = FMath::Abs(SlotIndex - j);
		Best = FMath::Min(Best, FMath::Min(Raw, NumSlots - Raw));
	}
	return bAnyOccupied ? Best : NumSlots;
}

void UEncirclementSubsystem::DrawDebug() const
{
	// 기본 반경만 표시하므로 실제 DT 기반 ChaseLocation과 다를 수 있다.
	// 실제 목적지는 Debug 1, 참고용 링과 슬롯은 Debug 2 이상에서 표시한다.
	if (CVarEncircleDebug.GetValueOnGameThread() < 2)
	{
		return;
	}

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

		DrawDebugCircle(World, TargetLoc, InnerRadius, 32, FColor::Yellow, false, -1.f, 0, 1.f, FVector(1, 0, 0), FVector(0, 1, 0), false);
		
		DrawDebugCircle(World, TargetLoc, OuterRadius, 32, FColor::Orange, false, -1.f, 0, 1.f, FVector(1, 0, 0), FVector(0, 1, 0), false);

		for (int32 i = 0; i < NumSlots; ++i)
		{
			if (Ring.Slots.IsValidIndex(i) && Ring.Slots[i].IsValid())
			{
				AActor* Shifter = Ring.Slots[i].Get();
				
				bool bHasToken = Ring.AttackTokens.Contains(Shifter);
				
				FVector SlotLoc = GetSlotLocation(Target, i, !bHasToken /* 토큰 없으면 아우터 위치 */);
				
				DrawDebugSphere(World, SlotLoc, 20.f, 8, bHasToken ? FColor::Red : FColor::Cyan, false, -1.f);
				DrawDebugLine(World, TargetLoc, SlotLoc, bHasToken ? FColor::Red : FColor::Cyan, false, -1.f);
			}
		}
	}
}
