#include "Subsystems/RetrieveObjectiveMarkerSubsystem.h"

#include "Components/World/RetrieveObjectiveAnchorComponent.h"
#include "Data/RetrieveMapConfigDataAsset.h"
#include "Data/RetrieveObjectiveAnchorDataAsset.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Subsystems/RetrieveMapSubsystem.h"
#include "World/RetrieveAreaClearQuestActor.h"
#include "World/RetrieveStoryTriggerVolume.h"

const FName URetrieveObjectiveMarkerSubsystem::TrackedQuestMarkerId(TEXT("TrackedQuestStep"));

// ─────────────────────────────────────────────────────────────────────────────
// 마커 등록/해제
// ─────────────────────────────────────────────────────────────────────────────
void URetrieveObjectiveMarkerSubsystem::RegisterMarker(FRetrieveObjectiveMarker&& InMarker)
{
	if (InMarker.MarkerId.IsNone())
	{
		return;
	}

	if (FRetrieveObjectiveMarker* Existing = FindMarker(InMarker.MarkerId))
	{
		*Existing = MoveTemp(InMarker);
		return;
	}

	Markers.Add(MoveTemp(InMarker));
}

void URetrieveObjectiveMarkerSubsystem::RemoveMarker(FName MarkerId)
{
	Markers.RemoveAll([MarkerId](const FRetrieveObjectiveMarker& Marker)
	{
		return Marker.MarkerId == MarkerId;
	});
}

void URetrieveObjectiveMarkerSubsystem::RemoveMarkersWithPrefix(const FString& Prefix)
{
	if (Prefix.IsEmpty())
	{
		return;
	}

	Markers.RemoveAll([&Prefix](const FRetrieveObjectiveMarker& Marker)
	{
		return Marker.MarkerId.ToString().StartsWith(Prefix, ESearchCase::CaseSensitive);
	});
}

void URetrieveObjectiveMarkerSubsystem::GetScreenMarkers(TArray<const FRetrieveObjectiveMarker*>& OutMarkers) const
{
	OutMarkers.Reset();
	for (const FRetrieveObjectiveMarker& Marker : Markers)
	{
		if (Marker.State.bVisible && Marker.State.bAllowScreenMarker)
		{
			OutMarkers.Add(&Marker);
		}
	}
}

void URetrieveObjectiveMarkerSubsystem::GetAcceptedQuestMarkers(
	const FVector& ViewerLocation,
	TArray<const FRetrieveObjectiveMarker*>& OutMarkers,
	bool bRequireCurrentProximity) const
{
	OutMarkers.Reset();

	for (const FRetrieveObjectiveMarker& Marker : Markers)
	{
		// Offer = 아직 말도 안 걸어본 의뢰, Main = 메인 퀘스트. 둘 다 "받아둔 의뢰"가 아니다.
		if (Marker.Kind != ERetrieveObjectiveMarkerKind::Side &&
			Marker.Kind != ERetrieveObjectiveMarkerKind::TurnIn)
		{
			continue;
		}
		if (!Marker.State.bVisible)
		{
			continue;
		}

		if (bRequireCurrentProximity)
		{
			// 트래커: 지금 가까이 있는 것만. 구출형처럼 수락 없이 진행 중이 되는 퀘스트가
			// 맵 반대편에서부터 잡히는 것을 막는다.
			if (!PassesDiscoveryRadius(Marker, ViewerLocation, DiscoveryRadius))
			{
				continue;
			}
		}
		else if (!DiscoveredMarkerIds.Contains(Marker.MarkerId))
		{
			// 저널: 한 번이라도 발견한 의뢰는 목록에 남긴다.
			continue;
		}

		OutMarkers.Add(&Marker);
	}

	OutMarkers.Sort([&ViewerLocation](const FRetrieveObjectiveMarker& A, const FRetrieveObjectiveMarker& B)
	{
		return FVector::DistSquared(ViewerLocation, A.State.WorldLocation)
			 < FVector::DistSquared(ViewerLocation, B.State.WorldLocation);
	});
}

const FRetrieveObjectiveMarker* URetrieveObjectiveMarkerSubsystem::FindMarkerById(FName MarkerId) const
{
	return Markers.FindByPredicate([MarkerId](const FRetrieveObjectiveMarker& Marker)
	{
		return Marker.MarkerId == MarkerId;
	});
}

FRetrieveObjectiveMarker* URetrieveObjectiveMarkerSubsystem::FindMarker(FName MarkerId)
{
	return Markers.FindByPredicate([MarkerId](const FRetrieveObjectiveMarker& Marker)
	{
		return Marker.MarkerId == MarkerId;
	});
}

// ─────────────────────────────────────────────────────────────────────────────
// 메인/사이드 추적 목표
// ─────────────────────────────────────────────────────────────────────────────
void URetrieveObjectiveMarkerSubsystem::SetTrackedQuestSteps(const TArray<FTrackedStep>& Steps)
{
	bool bSame = Steps.Num() == ActiveSteps.Num();
	if (bSame)
	{
		for (int32 i = 0; i < Steps.Num(); ++i)
		{
			if (!Steps[i].StepTag.MatchesTagExact(ActiveSteps[i].StepTag)
				|| Steps[i].Kind != ActiveSteps[i].Kind
				|| !Steps[i].Label.EqualTo(ActiveSteps[i].Label))
			{
				bSame = false;
				break;
			}
		}
	}

	if (bSame)
	{
		return;
	}

	ActiveSteps = Steps;
	RefreshTrackedQuestMarker();
}

void URetrieveObjectiveMarkerSubsystem::ClearTrackedQuestStep()
{
	if (ActiveSteps.Num() == 0)
	{
		return;
	}

	ActiveSteps.Reset();
	RemoveMarkersWithPrefix(TrackedQuestMarkerId.ToString());
}

void URetrieveObjectiveMarkerSubsystem::RefreshTrackedQuestMarker()
{
	RemoveMarkersWithPrefix(TrackedQuestMarkerId.ToString());

	if (ActiveSteps.Num() == 0)
	{
		return;
	}

	// 같은 스텝 태그를 여러 액터가 주장할 수 있다(예: 늑대 처치 = 스토리 볼륨 + 구역클리어 액터).
	// "먼저 등록된 것"을 쓰면 스트리밍 순서에 따라 엉뚱한 곳을 가리키므로,
	// 우선순위(과제 지점에 가까운 종류일수록 높음) → 동점이면 플레이어에 가까운 순으로 고른다.
	const AActor* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	const FVector PlayerLocation = PlayerPawn ? PlayerPawn->GetActorLocation() : FVector::ZeroVector;

	for (const FTrackedStep& Step : ActiveSteps)
	{
		if (!Step.StepTag.IsValid())
		{
			continue;
		}

		// 목표별로 고유 ID를 준다 — 필수/선택이 동시에 떠야 하므로 하나로 덮어쓰면 안 된다.
		const FName MarkerId(*FString::Printf(TEXT("%s.%s"),
			*TrackedQuestMarkerId.ToString(), *Step.StepTag.ToString()));

		// 선택 목표는 시선 우선순위를 낮춰 필수 목표가 먼저 읽히게 한다.
		const bool bOptional = Step.Kind == ERetrieveObjectiveMarkerKind::MainOptional;
		const int32 SortPriority = bOptional ? 90 : 100;

		URetrieveObjectiveAnchorComponent* Match = nullptr;
		int32 BestPriority = MIN_int32;
		float BestDistSq = TNumericLimits<float>::Max();

		for (const TWeakObjectPtr<URetrieveObjectiveAnchorComponent>& WeakAnchor : Anchors)
		{
			URetrieveObjectiveAnchorComponent* Anchor = WeakAnchor.Get();
			if (!Anchor || !Anchor->ObjectiveStepTag.MatchesTagExact(Step.StepTag))
			{
				continue;
			}

			const int32 Priority = GetAnchorPriority(Anchor);
			const float DistSq = FVector::DistSquared(PlayerLocation, Anchor->GetMarkerWorldLocation());

			if (Priority > BestPriority || (Priority == BestPriority && DistSq < BestDistSq))
			{
				Match = Anchor;
				BestPriority = Priority;
				BestDistSq = DistSq;
			}
		}

		if (!Match)
		{
			// 앵커가 아직 스트리밍되지 않았다 → 에디터에서 구워둔 좌표로 마커를 띄운다.
			// 메인 퀘스트는 "멀리 있어도 항상 보여야" 하므로 여기서 끊기면 안 된다.
			// 실제 앵커가 로드되면 RegisterAnchor가 이 함수를 다시 돌려 정확한 위치로 교체한다.
			FVector BakedLocation;
			const URetrieveObjectiveAnchorDataAsset* Baked = GetBakedAnchorData();
			if (!Baked || !Baked->FindAnchorLocation(Step.StepTag, BakedLocation))
			{
				continue;
			}

			FRetrieveObjectiveMarker BakedMarker;
			BakedMarker.MarkerId = MarkerId;
			BakedMarker.Kind = Step.Kind;
			BakedMarker.Label = Step.Label;
			BakedMarker.SortPriority = SortPriority;
			BakedMarker.State.WorldLocation = BakedLocation;
			// 구운 좌표는 "그 지역"을 가리키는 값이므로 대략적 표시로 둔다.
			BakedMarker.State.bApproximate = true;

			RegisterMarker(MoveTemp(BakedMarker));
			continue;
		}

		FRetrieveObjectiveMarker Marker;
		Marker.MarkerId = MarkerId;
		Marker.Kind = Step.Kind;
		Marker.Label = Match->MarkerLabelOverride.IsEmptyOrWhitespace() ? Step.Label : Match->MarkerLabelOverride;
		Marker.SortPriority = SortPriority;
		Marker.State.WorldLocation = Match->GetMarkerWorldLocation();

		Marker.RefreshDelegate.BindWeakLambda(Match,
			[WeakAnchor = TWeakObjectPtr<URetrieveObjectiveAnchorComponent>(Match)](FRetrieveObjectiveMarkerState& State)
			{
				const URetrieveObjectiveAnchorComponent* Anchor = WeakAnchor.Get();
				if (!Anchor)
				{
					return false;
				}
				State.WorldLocation = Anchor->GetMarkerWorldLocation();
				return true;
			});

		RegisterMarker(MoveTemp(Marker));
	}
}

bool URetrieveObjectiveMarkerSubsystem::PassesDiscoveryRadius(
	const FRetrieveObjectiveMarker& Marker, const FVector& ViewerLocation, float Radius)
{
	// 메인 계열(필수·선택)은 거리와 무관하게 항상 보인다.
	if (Marker.Kind == ERetrieveObjectiveMarkerKind::Main
		|| Marker.Kind == ERetrieveObjectiveMarkerKind::MainOptional)
	{
		return true;
	}
	if (Radius <= 0.0f)
	{
		return true; // 반경 0 = 제한 없음
	}

	return FVector::DistSquared(ViewerLocation, Marker.State.WorldLocation) <= FMath::Square(Radius);
}

int32 URetrieveObjectiveMarkerSubsystem::GetAnchorPriority(const URetrieveObjectiveAnchorComponent* Anchor)
{
	// 같은 스텝을 여러 액터가 주장할 때 "과제가 실제로 있는 곳"을 고르기 위한 서열.
	//   직접 부착(수호자·코어·봉인 등)  : 가장 명시적이므로 최우선
	//   구역 클리어 액터                : 처치 대상 근처에 놓이므로 신뢰도 높음
	//   스토리 볼륨                     : "완료를 판정하는 지점"일 뿐 과제 지점이 아닐 수 있어 최후 수단
	const AActor* Owner = Anchor ? Anchor->GetOwner() : nullptr;
	if (!Owner)
	{
		return MIN_int32;
	}

	if (Owner->IsA<ARetrieveStoryTriggerVolume>())
	{
		return -10;
	}
	if (Owner->IsA<ARetrieveAreaClearQuestActor>())
	{
		return 10;
	}
	return 20;
}

bool URetrieveObjectiveMarkerSubsystem::PassesMapVisibility(const FRetrieveObjectiveMarker& Marker) const
{
	// 메인 계열(필수·선택)은 아직 안 가본 곳이어도 맵에 표시한다.
	if (Marker.Kind == ERetrieveObjectiveMarkerKind::Main
		|| Marker.Kind == ERetrieveObjectiveMarkerKind::MainOptional)
	{
		return true;
	}
	return DiscoveredMarkerIds.Contains(Marker.MarkerId);
}

void URetrieveObjectiveMarkerSubsystem::UpdateDiscoveredMarkers()
{
	if (Markers.Num() == 0)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const APawn* Pawn = World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
	if (!Pawn)
	{
		return;
	}

	const FVector PlayerLocation = Pawn->GetActorLocation();
	for (const FRetrieveObjectiveMarker& Marker : Markers)
	{
		if (DiscoveredMarkerIds.Contains(Marker.MarkerId))
		{
			continue;
		}
		if (PassesDiscoveryRadius(Marker, PlayerLocation, DiscoveryRadius))
		{
			DiscoveredMarkerIds.Add(Marker.MarkerId);
		}
	}
}

void URetrieveObjectiveMarkerSubsystem::RegisterActorMarker(
	UWorld* World,
	FName MarkerId,
	ERetrieveObjectiveMarkerKind Kind,
	const FText& Label,
	AActor* Target,
	const FText& ProgressText,
	float ZOffset,
	int32 SortPriority)
{
	URetrieveObjectiveMarkerSubsystem* MarkerSub =
		World ? World->GetSubsystem<URetrieveObjectiveMarkerSubsystem>() : nullptr;
	if (!MarkerSub || !IsValid(Target) || MarkerId.IsNone())
	{
		return;
	}

	FRetrieveObjectiveMarker Marker;
	Marker.MarkerId = MarkerId;
	Marker.Kind = Kind;
	Marker.Label = Label;
	Marker.SortPriority = SortPriority;
	Marker.State.WorldLocation = Target->GetActorLocation() + FVector(0.0f, 0.0f, ZOffset);
	Marker.State.ProgressText = ProgressText;

	Marker.RefreshDelegate.BindWeakLambda(Target,
		[WeakTarget = TWeakObjectPtr<AActor>(Target), ZOffset](FRetrieveObjectiveMarkerState& State)
		{
			const AActor* Actor = WeakTarget.Get();
			if (!Actor)
			{
				return false; // 대상이 사라지면 마커도 사라진다.
			}
			State.WorldLocation = Actor->GetActorLocation() + FVector(0.0f, 0.0f, ZOffset);
			return true;
		});

	MarkerSub->RegisterMarker(MoveTemp(Marker));
}

void URetrieveObjectiveMarkerSubsystem::RemoveMarkersByPrefix(UWorld* World, const FString& Prefix)
{
	if (URetrieveObjectiveMarkerSubsystem* MarkerSub =
		World ? World->GetSubsystem<URetrieveObjectiveMarkerSubsystem>() : nullptr)
	{
		MarkerSub->RemoveMarkersWithPrefix(Prefix);
	}
}

const URetrieveObjectiveAnchorDataAsset* URetrieveObjectiveMarkerSubsystem::GetBakedAnchorData()
{
	if (BakedAnchorData)
	{
		return BakedAnchorData;
	}
	if (bBakedAnchorLookupDone)
	{
		return nullptr; // 미연결 — 매 갱신마다 다시 찾지 않는다.
	}

	UWorld* World = GetWorld();
	URetrieveMapSubsystem* MapSub = World ? World->GetSubsystem<URetrieveMapSubsystem>() : nullptr;
	if (!MapSub)
	{
		return nullptr; // 맵 서브시스템이 아직 없으면 다음 기회에 다시 시도한다.
	}

	// 맵 설정은 월드맵을 한 번도 열지 않아도 로드되도록 보장돼 있다.
	MapSub->EnsureMapConfigLoaded();
	if (!MapSub->MapConfig)
	{
		return nullptr; // 아직 로드 전 — 다음 갱신에서 다시 시도한다.
	}

	BakedAnchorData = MapSub->MapConfig->ObjectiveAnchorData;
	bBakedAnchorLookupDone = true; // 슬롯이 비어 있어도 재조회하지 않는다.
	return BakedAnchorData;
}

// ─────────────────────────────────────────────────────────────────────────────
// 앵커
// ─────────────────────────────────────────────────────────────────────────────
void URetrieveObjectiveMarkerSubsystem::RegisterAnchor(URetrieveObjectiveAnchorComponent* Anchor)
{
	if (!Anchor)
	{
		return;
	}

	Anchors.AddUnique(Anchor);

	// 이 앵커가 지금 추적 중인 스텝이라면 즉시 마커를 붙인다(WP 스트리밍 인 대응).
	for (const FTrackedStep& Step : ActiveSteps)
	{
		if (Anchor->ObjectiveStepTag.MatchesTagExact(Step.StepTag))
		{
			RefreshTrackedQuestMarker();
			break;
		}
	}
}

void URetrieveObjectiveMarkerSubsystem::UnregisterAnchor(URetrieveObjectiveAnchorComponent* Anchor)
{
	if (!Anchor)
	{
		return;
	}

	Anchors.Remove(Anchor);

	for (const FTrackedStep& Step : ActiveSteps)
	{
		if (Anchor->ObjectiveStepTag.MatchesTagExact(Step.StepTag))
		{
			RefreshTrackedQuestMarker();
			break;
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Tick
// ─────────────────────────────────────────────────────────────────────────────
void URetrieveObjectiveMarkerSubsystem::Tick(float DeltaTime)
{
	// UTickableWorldSubsystem::Tick은 순수 가상이므로 Super 호출을 하지 않는다.
	if (Markers.Num() == 0)
	{
		return;
	}

	RefreshAccumulator += DeltaTime;
	if (RefreshAccumulator < RefreshInterval)
	{
		return;
	}
	RefreshAccumulator = 0.0f;

	// 갱신 중 배열이 바뀌지 않도록 제거 대상을 모았다가 뒤에서 지운다.
	TArray<FName, TInlineAllocator<4>> Expired;

	for (FRetrieveObjectiveMarker& Marker : Markers)
	{
		if (!Marker.RefreshDelegate.IsBound())
		{
			continue;
		}

		FRetrieveObjectiveMarkerState NewState = Marker.State;
		if (Marker.RefreshDelegate.Execute(NewState))
		{
			Marker.State = NewState;
		}
		else
		{
			Expired.Add(Marker.MarkerId);
		}
	}

	for (const FName& MarkerId : Expired)
	{
		RemoveMarker(MarkerId);
	}

	// 갱신된 위치 기준으로 "발견함" 기록을 남긴다(맵은 이 기록으로 계속 표시).
	UpdateDiscoveredMarkers();
}

TStatId URetrieveObjectiveMarkerSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URetrieveObjectiveMarkerSubsystem, STATGROUP_Tickables);
}

bool URetrieveObjectiveMarkerSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void URetrieveObjectiveMarkerSubsystem::Deinitialize()
{
	Markers.Reset();
	Anchors.Reset();
	Super::Deinitialize();
}
