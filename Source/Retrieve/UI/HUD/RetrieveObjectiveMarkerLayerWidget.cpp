#include "UI/HUD/RetrieveObjectiveMarkerLayerWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "TimerManager.h"
#include "Player/RetrievePlayerController.h"
#include "Subsystems/RetrieveObjectiveMarkerSubsystem.h"
#include "UI/HUD/RetrieveObjectiveMarkerWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogObjectiveMarker, Log, All);

UCanvasPanel* URetrieveObjectiveMarkerLayerWidget::ResolveMarkerCanvas()
{
	if (CanvasPanel_Markers)
	{
		return CanvasPanel_Markers;
	}

	// BindWidgetOptional이 이름/Is Variable 문제로 비어 있어도 동작하도록,
	// 루트가 CanvasPanel이면 그것을 쓰고, 아니면 트리에서 첫 CanvasPanel을 찾는다.
	if (UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(GetRootWidget()))
	{
		return RootCanvas;
	}

	if (WidgetTree)
	{
		UCanvasPanel* FoundCanvas = nullptr;
		WidgetTree->ForEachWidget([&FoundCanvas](UWidget* Widget)
		{
			if (!FoundCanvas)
			{
				FoundCanvas = Cast<UCanvasPanel>(Widget);
			}
		});
		return FoundCanvas;
	}

	return nullptr;
}

void URetrieveObjectiveMarkerLayerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 마커 레이어는 입력을 절대 먹으면 안 된다(전투 중 클릭/커서 판정 방해 방지).
	SetVisibility(ESlateVisibility::HitTestInvisible);

	ResolvedCanvas = ResolveMarkerCanvas();

	UE_LOG(LogObjectiveMarker, Log,
		TEXT("[ObjectiveMarkerLayer] Construct — Canvas=%s(bound=%s), MarkerWidgetClass=%s"),
		ResolvedCanvas ? *ResolvedCanvas->GetName() : TEXT("NULL"),
		CanvasPanel_Markers ? TEXT("yes") : TEXT("no(fallback)"),
		*GetNameSafe(MarkerWidgetClass));

	if (!ResolvedCanvas || !MarkerWidgetClass)
	{
		UE_LOG(LogObjectiveMarker, Error,
			TEXT("[ObjectiveMarkerLayer] 화면 마커를 그릴 수 없습니다. "
			     "WBP에 CanvasPanel이 있는지, MarkerWidgetClass가 지정됐는지 확인하세요."));
	}

	if (UWorld* World = GetWorld())
	{
		ReminderHandle = UGameplayMessageSubsystem::Get(World).RegisterListener<FRetrieveObjectiveReminderPayload>(
			RetrieveGameplayTags::Channel_UI_ObjectiveReminder,
			this, &URetrieveObjectiveMarkerLayerWidget::HandleObjectiveReminder);

		// UUserWidget은 TickFrequency=Auto일 때 BP Tick/애니메이션이 없으면 틱을 받지 못할 수 있다.
		// 1초 뒤에도 NativeTick이 한 번도 안 왔으면 타이머로 갱신을 대신 돌린다.
		World->GetTimerManager().SetTimer(
			TickWatchdogHandle, FTimerDelegate::CreateUObject(
				this, &URetrieveObjectiveMarkerLayerWidget::CheckTickWatchdog), 1.0f, false);
	}
}

void URetrieveObjectiveMarkerLayerWidget::CheckTickWatchdog()
{
	if (bTickReceived)
	{
		return; // 정상적으로 틱을 받고 있다.
	}

	UE_LOG(LogObjectiveMarker, Warning,
		TEXT("[ObjectiveMarkerLayer] NativeTick이 오지 않아 타이머 갱신으로 전환합니다."));

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			FallbackUpdateHandle, FTimerDelegate::CreateUObject(
				this, &URetrieveObjectiveMarkerLayerWidget::UpdateFromCachedGeometry),
			1.0f / 60.0f, true);
	}
}

void URetrieveObjectiveMarkerLayerWidget::UpdateFromCachedGeometry()
{
	UpdateMarkers(GetCachedGeometry());
}

void URetrieveObjectiveMarkerLayerWidget::HandleObjectiveReminder(
	FGameplayTag /*Channel*/, const FRetrieveObjectiveReminderPayload& /*Message*/)
{
	ReplayAppearEffects();
}

void URetrieveObjectiveMarkerLayerWidget::ReplayAppearEffects()
{
	// 등장 시각을 지금으로 되돌리면 다음 틱부터 스케일 인이 다시 재생된다.
	const double Now = FPlatformTime::Seconds();
	for (TPair<FName, double>& Pair : MarkerAppearTimes)
	{
		Pair.Value = Now;
	}

	for (const TPair<FName, TObjectPtr<URetrieveObjectiveMarkerWidget>>& Pair : MarkerWidgetPool)
	{
		if (URetrieveObjectiveMarkerWidget* MarkerWidget = Pair.Value.Get())
		{
			MarkerWidget->PlayAppearEffect();
		}
	}
}

void URetrieveObjectiveMarkerLayerWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		if (ReminderHandle.IsValid())
		{
			UGameplayMessageSubsystem::Get(World).UnregisterListener(ReminderHandle);
		}
	}
	ReminderHandle = FGameplayMessageListenerHandle();

	MarkerWidgetPool.Reset();
	MarkerAppearTimes.Reset();
	Super::NativeDestruct();
}

URetrieveObjectiveMarkerSubsystem* URetrieveObjectiveMarkerLayerWidget::GetMarkerSubsystem() const
{
	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<URetrieveObjectiveMarkerSubsystem>() : nullptr;
}

// 화면 마커의 거리 컬링도 공용 규칙을 그대로 쓴다(중복 로직 제거).

bool URetrieveObjectiveMarkerLayerWidget::ShouldHideAllMarkers() const
{
	if (bSuppressedByRequest)
	{
		return true;
	}

	if (bHideWhilePanelOpen)
	{
		if (const ARetrievePlayerController* PC = Cast<ARetrievePlayerController>(GetOwningPlayer()))
		{
			if (PC->GetActivePanel() != nullptr)
			{
				return true;
			}
		}
	}

	return false;
}

void URetrieveObjectiveMarkerLayerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	bTickReceived = true;
	UpdateMarkers(MyGeometry);
}

void URetrieveObjectiveMarkerLayerWidget::UpdateMarkers(const FGeometry& MyGeometry)
{
	if (!ResolvedCanvas)
	{
		ResolvedCanvas = ResolveMarkerCanvas();
	}

	if (!ResolvedCanvas || !MarkerWidgetClass)
	{
		return; // WBP 구성 미완료 — 조용히 아무것도 그리지 않는다.
	}

	if (MyGeometry.GetLocalSize().X <= 1.0f || MyGeometry.GetLocalSize().Y <= 1.0f)
	{
		return; // 아직 레이아웃 전(캐시 지오메트리가 0인 첫 프레임 등).
	}

	if (ShouldHideAllMarkers())
	{
		CollapseUnusedMarkers(TSet<FName>());
		return;
	}

	TArray<FResolvedMarker> Resolved;
	ResolveMarkers(MyGeometry, Resolved);
	ApplyDeclutter(Resolved);
	UpdateMarkerWidgets(Resolved);

	// 첫 갱신 결과를 한 번만 남긴다 — "마커가 안 보인다"를 로그로 구분하기 위함.
	if (!bLoggedFirstUpdate)
	{
		bLoggedFirstUpdate = true;
		const URetrieveObjectiveMarkerSubsystem* MarkerSub = GetMarkerSubsystem();
		UE_LOG(LogObjectiveMarker, Log,
			TEXT("[ObjectiveMarkerLayer] 첫 갱신 — 등록 마커=%d, 화면 표시=%d, 위젯크기=%.0fx%.0f"),
			MarkerSub ? MarkerSub->GetMarkers().Num() : -1,
			Resolved.Num(),
			MyGeometry.GetLocalSize().X, MyGeometry.GetLocalSize().Y);
	}
}

void URetrieveObjectiveMarkerLayerWidget::ResolveMarkers(
	const FGeometry& MyGeometry, TArray<FResolvedMarker>& OutResolved) const
{
	OutResolved.Reset();

	URetrieveObjectiveMarkerSubsystem* MarkerSub = GetMarkerSubsystem();
	APlayerController* PC = GetOwningPlayer();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!MarkerSub || !PC || !Pawn)
	{
		return;
	}

	// 나침반/미니맵/월드맵이 같은 발견 반경을 쓰도록 이 위젯 설정값을 공유한다.
	MarkerSub->SetDiscoveryRadius(SideMarkerViewDistance);

	int32 ViewportX = 0;
	int32 ViewportY = 0;
	PC->GetViewportSize(ViewportX, ViewportY);
	if (ViewportX <= 0 || ViewportY <= 0)
	{
		return;
	}

	const FVector2D LocalSize = MyGeometry.GetLocalSize();
	const FVector2D ViewportSize(static_cast<float>(ViewportX), static_cast<float>(ViewportY));
	const FVector2D Center = LocalSize * 0.5f;

	FVector CameraLocation = FVector::ZeroVector;
	FRotator CameraRotation = FRotator::ZeroRotator;
	PC->GetPlayerViewPoint(CameraLocation, CameraRotation);
	const FVector CameraForward = CameraRotation.Vector();

	const FVector PlayerLocation = Pawn->GetActorLocation();

	for (const FRetrieveObjectiveMarker& Marker : MarkerSub->GetMarkers())
	{
		if (!Marker.State.bVisible || !Marker.State.bAllowScreenMarker)
		{
			continue;
		}

		const bool bIsMain = Marker.Kind == ERetrieveObjectiveMarkerKind::Main
			|| Marker.Kind == ERetrieveObjectiveMarkerKind::MainOptional;
		const float Distance = FVector::Dist(PlayerLocation, Marker.State.WorldLocation);

		// 메인 목표는 항상 유지. 인스턴스 마커는 발견 반경 안에서만(맵들과 동일 규칙).
		if (!URetrieveObjectiveMarkerSubsystem::PassesDiscoveryRadius(
				Marker, PlayerLocation, SideMarkerViewDistance))
		{
			continue;
		}

		FResolvedMarker Entry;
		Entry.MarkerId = Marker.MarkerId;
		Entry.Kind = Marker.Kind;
		Entry.Label = Marker.Label;
		Entry.ProgressText = Marker.State.ProgressText;
		Entry.DistanceUU = Distance;
		Entry.bApproximate = Marker.State.bApproximate;
		Entry.SortPriority = Marker.SortPriority;
		Entry.bShowLabel = LabelVisibleDistance <= 0.0f || Distance <= LabelVisibleDistance;

		const FVector ToTarget = Marker.State.WorldLocation - CameraLocation;
		const bool bInFront = FVector::DotProduct(CameraForward, ToTarget) > 0.0f;

		FVector2D ScreenPos = FVector2D::ZeroVector;
		const bool bProjected = bInFront
			&& UGameplayStatics::ProjectWorldToScreen(PC, Marker.State.WorldLocation, ScreenPos, false);

		FVector2D LocalPos;
		if (bProjected)
		{
			// 뷰포트 픽셀 → 이 위젯의 로컬 좌표(DPI 스케일 차이를 비율로 흡수).
			LocalPos = FVector2D(
				ScreenPos.X / ViewportSize.X * LocalSize.X,
				ScreenPos.Y / ViewportSize.Y * LocalSize.Y);
		}
		else
		{
			// 카메라 뒤 — 좌/우 중 목표가 있는 쪽 가장자리로 보낸다.
			const float RelativeYaw = FMath::FindDeltaAngleDegrees(
				CameraRotation.Yaw, ToTarget.Rotation().Yaw);
			LocalPos = Center + FVector2D(RelativeYaw >= 0.0f ? LocalSize.X : -LocalSize.X, 0.0f);
		}

		// 목표 지점 바로 위로 띄운다(지면/대상에 겹치지 않게).
		LocalPos += MarkerScreenOffset;

		// 3인칭 캐릭터가 서 있는 화면 중앙을 피해 위로 밀어 올린다.
		// 단, 이미 캐릭터 머리 위로 충분히 올라가 있으면 더 밀지 않는다.
		// (가까이 있는 목표는 원근 때문에 저절로 화면 위쪽에 잡히는데, 여기에 밀어 올리기까지
		//  더하면 마커가 화면 꼭대기로 날아가 버린다)
		if (bAvoidScreenCenter && AvoidCenterRadius > 0.0f)
		{
			const FVector2D FromCenter = LocalPos - Center;
			const bool bOverlapsCharacter =
				FMath::Abs(FromCenter.X) < AvoidCenterRadius && FromCenter.Y > -AvoidCenterRadius * 0.25f;

			if (bOverlapsCharacter)
			{
				const float Ratio = 1.0f - FMath::Clamp(FMath::Abs(FromCenter.X) / AvoidCenterRadius, 0.0f, 1.0f);
				LocalPos.Y -= AvoidCenterLift * Ratio;
			}
		}

		// 화면 밖이면 가장자리로 클램프하고 방향 각도를 계산한다.
		const FVector2D HalfExtent(
			FMath::Max(Center.X - ScreenEdgePadding, 1.0f),
			FMath::Max(Center.Y - ScreenEdgePadding, 1.0f));
		FVector2D Offset = LocalPos - Center;

		const bool bOutside = FMath::Abs(Offset.X) > HalfExtent.X || FMath::Abs(Offset.Y) > HalfExtent.Y;
		if (bOutside)
		{
			const float ScaleX = FMath::Abs(Offset.X) > KINDA_SMALL_NUMBER
				? HalfExtent.X / FMath::Abs(Offset.X) : TNumericLimits<float>::Max();
			const float ScaleY = FMath::Abs(Offset.Y) > KINDA_SMALL_NUMBER
				? HalfExtent.Y / FMath::Abs(Offset.Y) : TNumericLimits<float>::Max();
			Offset *= FMath::Min(ScaleX, ScaleY);

			Entry.bOffscreen = true;
			Entry.EdgeAngleDeg = FMath::RadiansToDegrees(FMath::Atan2(Offset.Y, Offset.X));
			Entry.bShowLabel = false; // 가장자리에서는 아이콘+거리만
		}

		// 가림 판정: 벽 뒤에 있으면 옅게 그려 "저 너머"라는 걸 알린다.
		// 화면 밖 마커는 어차피 가장자리 아이콘이라 트레이스를 생략한다.
		if (bTraceOcclusion && !Entry.bOffscreen)
		{
			FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(ObjectiveMarkerOcclusion), false, Pawn);
			TraceParams.AddIgnoredActor(Pawn);

			FHitResult Hit;
			Entry.bOccluded = GetWorld() && GetWorld()->LineTraceSingleByChannel(
				Hit, CameraLocation, Marker.State.WorldLocation, ECC_Visibility, TraceParams);
		}

		Entry.LocalPosition = Center + Offset;
		OutResolved.Add(MoveTemp(Entry));
	}

	// 메인 → 우선순위 → 가까운 순.
	OutResolved.Sort([](const FResolvedMarker& A, const FResolvedMarker& B)
	{
		const bool bMainA = A.Kind == ERetrieveObjectiveMarkerKind::Main;
		const bool bMainB = B.Kind == ERetrieveObjectiveMarkerKind::Main;
		if (bMainA != bMainB)
		{
			return bMainA;
		}
		if (A.SortPriority != B.SortPriority)
		{
			return A.SortPriority > B.SortPriority;
		}
		return A.DistanceUU < B.DistanceUU;
	});

	if (MaxScreenMarkers > 0 && OutResolved.Num() > MaxScreenMarkers)
	{
		OutResolved.SetNum(MaxScreenMarkers);
	}
}

void URetrieveObjectiveMarkerLayerWidget::ApplyDeclutter(TArray<FResolvedMarker>& InOutResolved) const
{
	if (DeclutterRadius <= 0.0f)
	{
		return;
	}

	const float RadiusSq = DeclutterRadius * DeclutterRadius;

	// 정렬이 이미 "중요/가까운 순"이므로 앞선 마커가 라벨을 가져간다.
	for (int32 i = 0; i < InOutResolved.Num(); ++i)
	{
		if (!InOutResolved[i].bShowLabel)
		{
			continue;
		}

		for (int32 j = 0; j < i; ++j)
		{
			if (!InOutResolved[j].bShowLabel)
			{
				continue;
			}
			if (FVector2D::DistSquared(InOutResolved[i].LocalPosition, InOutResolved[j].LocalPosition) <= RadiusSq)
			{
				InOutResolved[i].bShowLabel = false;
				break;
			}
		}
	}
}

void URetrieveObjectiveMarkerLayerWidget::UpdateMarkerWidgets(const TArray<FResolvedMarker>& Resolved)
{
	TSet<FName> ActiveIds;
	ActiveIds.Reserve(Resolved.Num());

	for (const FResolvedMarker& Entry : Resolved)
	{
		ActiveIds.Add(Entry.MarkerId);

		TObjectPtr<URetrieveObjectiveMarkerWidget>* Found = MarkerWidgetPool.Find(Entry.MarkerId);
		URetrieveObjectiveMarkerWidget* MarkerWidget = Found ? Found->Get() : nullptr;
		bool bJustCreated = false;

		if (!MarkerWidget)
		{
			MarkerWidget = CreateWidget<URetrieveObjectiveMarkerWidget>(GetOwningPlayer(), MarkerWidgetClass);
			if (!MarkerWidget)
			{
				continue;
			}

			UPanelSlot* PanelSlot = ResolvedCanvas->AddChild(MarkerWidget);
			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(PanelSlot))
			{
				CanvasSlot->SetAutoSize(true);
				CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			}

			MarkerWidgetPool.Add(Entry.MarkerId, MarkerWidget);
			bJustCreated = true;
		}

		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(MarkerWidget->Slot))
		{
			CanvasSlot->SetPosition(Entry.LocalPosition);
		}

		// 새로 생겼거나, 컬링됐다가 다시 보이게 된 마커는 등장 연출을 다시 재생한다.
		const double Now = FPlatformTime::Seconds();
		const bool bWasHidden = MarkerWidget->GetVisibility() == ESlateVisibility::Collapsed;
		const bool bAppearing = bJustCreated || bWasHidden || !MarkerAppearTimes.Contains(Entry.MarkerId);
		if (bAppearing)
		{
			MarkerAppearTimes.Add(Entry.MarkerId, Now);
		}

		const bool bIsMainKind = Entry.Kind == ERetrieveObjectiveMarkerKind::Main;

		FRetrieveObjectiveMarkerVisual Visual;
		Visual.Kind = Entry.Kind;
		// 인스턴스 계열은 "의뢰 · ○○"으로 표기해 메인과 텍스트로도 구분한다.
		Visual.Label = (!bIsMainKind && !Entry.Label.IsEmptyOrWhitespace())
			? FText::Format(INVTEXT("{0}{1}"), SideLabelPrefix, Entry.Label)
			: Entry.Label;
		Visual.ProgressText = Entry.ProgressText;
		Visual.DistanceMeters = Entry.DistanceUU / 100.0f; // UU → m
		Visual.bOffscreen = Entry.bOffscreen;
		Visual.EdgeAngleDeg = Entry.EdgeAngleDeg;
		Visual.bShowLabel = Entry.bShowLabel;
		Visual.bApproximate = Entry.bApproximate;
		Visual.bOccluded = Entry.bOccluded;

		// ── 등장 연출: 크게 나타났다가 제자리로. 퀘스트를 수락한 순간을 놓치지 않게 한다.
		const double Elapsed = Now - MarkerAppearTimes.FindRef(Entry.MarkerId);
		Visual.AppearProgress = AppearDuration > 0.0f
			? FMath::Clamp(static_cast<float>(Elapsed) / AppearDuration, 0.0f, 1.0f)
			: 1.0f;

		// EaseOut(1-(1-t)^3)으로 초반에 빠르게 줄어들어 "톡" 하고 자리잡는 느낌을 준다.
		const float EaseOut = 1.0f - FMath::Pow(1.0f - Visual.AppearProgress, 3.0f);
		float Scale = FMath::Lerp(AppearStartScale, 1.0f, EaseOut);

		// WBP 아트가 100px 기준이라 화면에서는 과하게 크다. 전체 기본 배율로 줄인다.
		Scale *= MarkerBaseScale;

		// ── 거리 스케일: 멀수록 작게 → 어떤 목표가 가까운지 크기만으로 구분된다.
		if (DistanceScaleFarRange > DistanceScaleNearRange)
		{
			const float DistAlpha = FMath::Clamp(
				(Entry.DistanceUU - DistanceScaleNearRange) / (DistanceScaleFarRange - DistanceScaleNearRange),
				0.0f, 1.0f);
			Scale *= FMath::Lerp(1.0f, DistanceScaleMin, DistAlpha);
		}

		// ── 크기 차등 + 호흡 펄스.
		// 상시 펄스는 메인에만 준다 — 움직이는 것이 하나뿐이어야 "지금 할 일"이 명확해진다.
		if (bIsMainKind)
		{
			Scale *= MainMarkerScale;

			if (IdlePulseAmplitude > 0.0f && IdlePulsePeriod > 0.0f)
			{
				const float Phase = static_cast<float>(FMath::Fmod(Now, static_cast<double>(IdlePulsePeriod)))
					/ IdlePulsePeriod;
				Scale *= 1.0f + IdlePulseAmplitude * FMath::Sin(Phase * 2.0f * PI);
			}
		}
		else
		{
			Scale *= SideMarkerScale;
		}

		Visual.ScaleMultiplier = Scale;

		// 목표 코앞에서는 마커가 시야를 가리지 않도록 옅어진다 —
		// 이 지점부터는 기존 상호작용 프롬프트가 안내를 이어받는다.
		Visual.Opacity = 1.0f;
		if (NearFadeDistance > 0.0f && Entry.DistanceUU < NearFadeDistance)
		{
			const float Alpha = FMath::Clamp(Entry.DistanceUU / NearFadeDistance, 0.0f, 1.0f);
			Visual.Opacity = FMath::Lerp(NearFadeMinAlpha, 1.0f, Alpha);
		}

		MarkerWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		MarkerWidget->ApplyVisual(Visual);

		if (bAppearing)
		{
			MarkerWidget->PlayAppearEffect();
			MarkerWidget->OnMarkerAppeared(Entry.Kind);
		}
	}

	CollapseUnusedMarkers(ActiveIds);
}

void URetrieveObjectiveMarkerLayerWidget::CollapseUnusedMarkers(const TSet<FName>& ActiveIds)
{
	TArray<FName> Stale;

	for (const TPair<FName, TObjectPtr<URetrieveObjectiveMarkerWidget>>& Pair : MarkerWidgetPool)
	{
		if (ActiveIds.Contains(Pair.Key))
		{
			continue;
		}

		if (URetrieveObjectiveMarkerWidget* MarkerWidget = Pair.Value.Get())
		{
			MarkerWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
		else
		{
			Stale.Add(Pair.Key);
		}
	}

	for (const FName& Key : Stale)
	{
		MarkerWidgetPool.Remove(Key);
		MarkerAppearTimes.Remove(Key);
	}
}
