#include "UI/Map/RetrieveCompassWidget.h"
#include "Subsystems/RetrieveMapSubsystem.h"
#include "Subsystems/RetrieveObjectiveMarkerSubsystem.h"
#include "UI/RetrieveUISettingsLibrary.h"
#include "Components/World/RetrieveMapIconComponent.h"
#include "Data/RetrieveMapIconRegistry.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Fonts/FontMeasure.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Blueprint/WidgetTree.h"
#include "Layout/Clipping.h"

// ── 내부 상수 ────────────────────────────────────────────────────────────────

namespace
{
	// 45° 간격 방위 (Yaw 기준: 0=N, 90=E, ±180=S, -90=W)
	struct FCompassDirection
	{
		float    Bearing;   // 도
		FString  Label;
		bool     bCardinal; // true = N/E/S/W
	};

	static const FCompassDirection GDirections[] =
	{
		{   0.0f, TEXT("N"),  true  },
		{  45.0f, TEXT("NE"), false },
		{  90.0f, TEXT("E"),  true  },
		{ 135.0f, TEXT("SE"), false },
		{ 180.0f, TEXT("S"),  true  },
		{ -135.0f, TEXT("SW"), false },
		{ -90.0f, TEXT("W"),  true  },
		{ -45.0f, TEXT("NW"), false },
	};
}

// ── 보조 함수 ────────────────────────────────────────────────────────────────

float URetrieveCompassWidget::BearingToX(
	float BearingDeg, float CameraYaw, float CompassWidth) const
{
	float RelAngle = BearingDeg - CameraYaw;

	// [-180, 180] 정규화
	while (RelAngle >  180.0f) { RelAngle -= 360.0f; }
	while (RelAngle < -180.0f) { RelAngle += 360.0f; }

	const float HalfFOV = FieldOfViewDeg * 0.5f;
	if (FMath::Abs(RelAngle) >= HalfFOV) { return -1.0f; } // 범위 밖

	// -HalfFOV → 0, +HalfFOV → CompassWidth
	return (RelAngle / FieldOfViewDeg + 0.5f) * CompassWidth;
}

void URetrieveCompassWidget::DrawCompassText(
	FSlateWindowElementList& OutDrawElements,
	int32& LayerId,
	const FGeometry& AllottedGeometry,
	const FString& Text,
	const FVector2D& CenterPos,
	const FSlateFontInfo& Font,
	const FLinearColor& Color
) const
{
	const TSharedRef<FSlateFontMeasure> FontMeasure =
		FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FVector2D TextSz = FontMeasure->Measure(Text, Font);
	const FVector2D DrawPos(CenterPos.X - TextSz.X * 0.5f, CenterPos.Y - TextSz.Y * 0.5f);

	// 드롭 섀도우
	FSlateDrawElement::MakeText(
		OutDrawElements,
		LayerId,
		AllottedGeometry.ToPaintGeometry(
			FVector2f(TextSz),
			FSlateLayoutTransform(FVector2f(DrawPos + FVector2D(1.0f, 1.0f)))
		),
		Text,
		Font,
		ESlateDrawEffect::None,
		FLinearColor(0.0f, 0.0f, 0.0f, 0.7f)
	);

	FSlateDrawElement::MakeText(
		OutDrawElements,
		++LayerId,
		AllottedGeometry.ToPaintGeometry(
			FVector2f(TextSz),
			FSlateLayoutTransform(FVector2f(DrawPos))
		),
		Text,
		Font,
		ESlateDrawEffect::None,
		Color
	);
}


// ── 위젯 마커 풀링 ───────────────────────────────────────────────────────────

namespace
{
	constexpr int32 CompassWorldIconMarkerKeyBase = 100000;
	constexpr int32 CompassWaypointMarkerKeyBase  = 200000;
	constexpr int32 CompassEnemyMarkerKeyBase     = 300000;
}

int32 URetrieveCompassWidget::GetStableEnemyMarkerKey(const URetrieveMapIconComponent* Icon)
{
	const FObjectKey Key(Icon);
	if (const int32* Found = EnemyIconSlots.Find(Key))
	{
		return CompassEnemyMarkerKeyBase + *Found;
	}

	const int32 NewSlot = NextEnemySlot++;
	EnemyIconSlots.Add(Key, NewSlot);
	return CompassEnemyMarkerKeyBase + NewSlot;
}

bool URetrieveCompassWidget::HasUsableMarkerPanel() const
{
	return bEnableMarkerWidgetPooling && IsValid(CanvasPanel_CompassMarkers);
}

TSubclassOf<UUserWidget> URetrieveCompassWidget::GetWidgetClassForIconType(ERetrieveMapIconType IconType) const
{
	// 에너미 마커는 정적 WorldMapIconData가 아니라 라이브 GetIcons() 경로(UpdateWidgetMarkers)가
	// 전담한다. 따라서 정적 경로에서는 EnemyIconTypes를 위젯 풀링 대상에서 제외한다.
	// (이중 표시 방지 — 가이드 STEP 4)
	if (EnemyIconTypes.Contains(IconType))
	{
		return nullptr;
	}

	// 정적 WorldMapIconData 전용 위젯 마커가 필요하면 여기에서 타입별로 반환한다.
	return nullptr;
}

bool URetrieveCompassWidget::ShouldUseWidgetMarker(ERetrieveMapIconType IconType) const
{
	return HasUsableMarkerPanel() && GetWidgetClassForIconType(IconType) != nullptr;
}

UUserWidget* URetrieveCompassWidget::GetOrCreatePooledMarker(
	const int32 MarkerKey,
	TSubclassOf<UUserWidget> WidgetClass
)
{
	if (!HasUsableMarkerPanel() || !WidgetClass)
	{
		return nullptr;
	}

	if (TObjectPtr<UUserWidget>* ExistingWidget = MarkerWidgetPool.Find(MarkerKey))
	{
		return ExistingWidget->Get();
	}

	UUserWidget* NewWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), WidgetClass);
	if (!IsValid(NewWidget))
	{
		return nullptr;
	}

	NewWidget->SetVisibility(ESlateVisibility::Collapsed);
	NewWidget->SetIsEnabled(true);
	NewWidget->SetRenderOpacity(1.0f);
	NewWidget->SetColorAndOpacity(FLinearColor::White);
	
	CanvasPanel_CompassMarkers->AddChild(NewWidget);
	if (UCanvasPanelSlot* NewSlot = Cast<UCanvasPanelSlot>(NewWidget->Slot))
	{
		NewSlot->SetZOrder(100);
	}
	MarkerWidgetPool.Add(MarkerKey, NewWidget);

	return NewWidget;
}

void URetrieveCompassWidget::SetPooledMarkerTransform(
	UUserWidget* MarkerWidget,
	const FVector2D& CenterPos,
	const FVector2D& Size
) const
{
	if (!IsValid(MarkerWidget))
	{
		return;
	}

	MarkerWidget->SetRenderOpacity(1.0f);
	MarkerWidget->SetColorAndOpacity(FLinearColor::White);

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(MarkerWidget->Slot))
	{
		CanvasSlot->SetAutoSize(false);
		CanvasSlot->SetAlignment(FVector2D(0.0f, 0.0f));
		CanvasSlot->SetPosition(CenterPos - Size * 0.5f);
		CanvasSlot->SetSize(Size);
		CanvasSlot->SetZOrder(100);
	}

	MarkerWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(MarkerWidget->Slot))
	{
		CanvasSlot->SetZOrder(100);
	}
}

void URetrieveCompassWidget::CollapseUnusedMarkers(const TSet<int32>& ActiveMarkerKeys)
{
	for (const TPair<int32, TObjectPtr<UUserWidget>>& Pair : MarkerWidgetPool)
	{
		if (!ActiveMarkerKeys.Contains(Pair.Key) && IsValid(Pair.Value))
		{
			Pair.Value->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void URetrieveCompassWidget::UpdateWidgetMarkers(const FGeometry& MyGeometry)
{
	if (!HasUsableMarkerPanel())
	{
		return;
	}

	const FVector2D Size = MyGeometry.GetLocalSize();
	const float Width = Size.X;
	const float Height = Size.Y;
	if (Width < 2.0f || Height < 2.0f)
	{
		CollapseUnusedMarkers(TSet<int32>());
		return;
	}

	const APlayerController* PC = GetOwningPlayer();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		CollapseUnusedMarkers(TSet<int32>());
		return;
	}

	const FVector PlayerLoc = Pawn->GetActorLocation();
	const float CameraYaw = SmoothedCameraYaw;

	const float BandCenterY = Height * FMath::Clamp(CompassBandYRatio, 0.0f, 1.0f);
	TSet<int32> ActiveMarkerKeys;

	if (WorldMapIconData && IconRegistry)
	{
		for (int32 IconIndex = 0; IconIndex < WorldMapIconData->Icons.Num(); ++IconIndex)
		{
			const FRetrieveMapIconEntry& Entry = WorldMapIconData->Icons[IconIndex];

			if (Entry.IconType == ERetrieveMapIconType::None ||
				Entry.IconType == ERetrieveMapIconType::Player ||
				EnemyIconTypes.Contains(Entry.IconType) ||
				HiddenIconTypesOnCompass.Contains(Entry.IconType))
			{
				continue;
			}

			TSubclassOf<UUserWidget> MarkerWidgetClass = GetWidgetClassForIconType(Entry.IconType);
			if (!MarkerWidgetClass)
			{
				continue;
			}

			const FVector Delta = Entry.WorldLocation - PlayerLoc;

			if (CompassIconViewRadius > 0.0f &&
				FVector::Dist2D(PlayerLoc, Entry.WorldLocation) > CompassIconViewRadius)
			{
				continue;
			}

			const float BearingDeg = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
			const float IconX = BearingToX(BearingDeg, CameraYaw, Width);
			if (IconX < 0.0f)
			{
				continue;
			}

			const FRetrieveMapIconRow& Row = IconRegistry->FindRow(Entry.IconType);
			const float IconSize = FMath::Max(Row.IconSize * CompassIconSizeScale, 8.0f);
			const FVector2D IconSize2D(IconSize, IconSize);
			const FVector2D IconCenter(IconX + WidgetMarkerOffset.X, BandCenterY + WidgetMarkerOffset.Y);

			const int32 MarkerKey = CompassWorldIconMarkerKeyBase + IconIndex;
			if (UUserWidget* MarkerWidget = GetOrCreatePooledMarker(MarkerKey, MarkerWidgetClass))
			{
				SetPooledMarkerTransform(MarkerWidget, IconCenter, IconSize2D);
				ActiveMarkerKeys.Add(MarkerKey);
			}
		}
	}

	// ── 라이브 에너미 마커 (미니맵과 동일 소스 GetIcons()) ──────────────────────
	if (EnemyMarkerWidgetClass)
	{
		UWorld* World = GetWorld();
		URetrieveMapSubsystem* MapSub = World ? World->GetSubsystem<URetrieveMapSubsystem>() : nullptr;

		if (MapSub)
		{
			for (const URetrieveMapIconComponent* Icon : MapSub->GetIcons())
			{
				if (!IsValid(Icon) || !IsValid(Icon->GetOwner()))      { continue; }
				if (!Icon->bShowOnMinimap)                             { continue; }
				if (!EnemyIconTypes.Contains(Icon->IconType))          { continue; }
				if (HiddenIconTypesOnCompass.Contains(Icon->IconType)) { continue; }

				// 오버라이드 아이콘(BP에서 등록한 커스텀 텍스처)은 고정 위젯인 EnemyMarkerWidgetClass로
				// 표현할 수 없으므로 NativePaint의 라이브 오버라이드 드로우 경로가 전담한다 (중복 방지).
				if (Icon->bOverrideIcon)                               { continue; }

				const FVector IconWorld = Icon->GetOwner()->GetActorLocation();

				if (EnemyViewRadius > 0.0f &&
					FVector::Dist2D(PlayerLoc, IconWorld) > EnemyViewRadius)
				{
					continue;
				}

				const FVector Delta = IconWorld - PlayerLoc;
				const float BearingDeg = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
				const float IconX = BearingToX(BearingDeg, CameraYaw, Width);
				if (IconX < 0.0f)
				{
					continue;
				}

				// 크기: Registry → 폴백 (오버라이드 개체는 위에서 이미 걸러짐)
				float IconSize = EnemyMarkerSize;
				if (IconRegistry)
				{
					const FRetrieveMapIconRow& Row = IconRegistry->FindRow(Icon->IconType);
					IconSize = FMath::Max(Row.IconSize * CompassIconSizeScale, 8.0f);
				}

				const FVector2D IconSize2D(IconSize, IconSize);
				const FVector2D IconCenter(IconX + WidgetMarkerOffset.X, BandCenterY + WidgetMarkerOffset.Y);

				const int32 MarkerKey = GetStableEnemyMarkerKey(Icon);
				if (UUserWidget* MarkerWidget = GetOrCreatePooledMarker(MarkerKey, EnemyMarkerWidgetClass))
				{
					SetPooledMarkerTransform(MarkerWidget, IconCenter, IconSize2D);
					ActiveMarkerKeys.Add(MarkerKey);
				}
			}
		}
	}

	if (UserWaypointMarkerWidgetClass)
	{
		UWorld* World = GetWorld();
		URetrieveMapSubsystem* MapSub = World ? World->GetSubsystem<URetrieveMapSubsystem>() : nullptr;

		if (MapSub)
		{
			const TArray<FUserWaypoint>& Waypoints = MapSub->GetUserWaypoints();

			for (int32 WaypointIndex = 0; WaypointIndex < Waypoints.Num(); ++WaypointIndex)
			{
				const FUserWaypoint& WP = Waypoints[WaypointIndex];

				const FVector Delta = WP.WorldLocation - PlayerLoc;
				const float BearingDeg = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
				const float MarkerX = BearingToX(BearingDeg, CameraYaw, Width);
				if (MarkerX < 0.0f)
				{
					continue;
				}

				const FVector2D MarkerSize(WaypointMarkerSize, WaypointMarkerSize);
				const FVector2D MarkerCenter(MarkerX + WaypointWidgetMarkerOffset.X, BandCenterY + WaypointWidgetMarkerOffset.Y);

				const int32 MarkerKey = CompassWaypointMarkerKeyBase + FMath::Max(WP.WaypointId, 0);
				if (UUserWidget* MarkerWidget = GetOrCreatePooledMarker(MarkerKey, UserWaypointMarkerWidgetClass))
				{
					SetPooledMarkerTransform(MarkerWidget, MarkerCenter, MarkerSize);
					// 웨이포인트마다 부여된 색(WP.Color)으로 틴트 — 월드맵·미니맵과 색 연동.
					// (이전에는 위젯 고정 WaypointMarkerColor라 순서별 색 구분이 반영되지 않았다.)
					// 플러그인 마커 위젯(Map_FantasyWarrior_Icon_Objective_01)의 내부 이미지 브러시에
					// 골드 고정 틴트가 박혀 있어 WP.Color와 곱해지면 월드맵과 색이 달라진다.
					// 내부 이미지 틴트를 흰색으로 중화해 최종 색 = WP.Color(월드맵과 동일)로 만든다.
					if (UWidgetTree* Tree = MarkerWidget->WidgetTree)
					{
						Tree->ForEachWidget([](UWidget* Child)
						{
							if (UImage* Img = Cast<UImage>(Child))
							{
								if (Img->GetBrush().TintColor.GetSpecifiedColor() != FLinearColor::White)
								{
									Img->SetBrushTintColor(FSlateColor(FLinearColor::White));
								}
								if (Img->GetColorAndOpacity() != FLinearColor::White)
								{
									Img->SetColorAndOpacity(FLinearColor::White);
								}
							}
						});
					}
					MarkerWidget->SetColorAndOpacity(WP.Color);
					ActiveMarkerKeys.Add(MarkerKey);
				}
			}
		}
	}

	CollapseUnusedMarkers(ActiveMarkerKeys);
}

void URetrieveCompassWidget::UpdateSmoothedCameraYaw(const APlayerController* PC, float DeltaTime)
{
	if (!PC)
	{
		return;
	}

	const float RawYaw = PC->PlayerCameraManager
		? PC->PlayerCameraManager->GetCameraRotation().Yaw
		: PC->GetControlRotation().Yaw;

	const float RawYawRad = FMath::DegreesToRadians(RawYaw);
	const FVector2D RawDir(FMath::Cos(RawYawRad), FMath::Sin(RawYawRad));

	if (!bSmoothedYawInitialized)
	{
		SmoothedYawDir = RawDir;
		SmoothedCameraYaw = RawYaw;
		bSmoothedYawInitialized = true;
		return;
	}

	// 각도를 직접 보간(FInterpTo/FindDeltaAngleDegrees)하는 방식은 ±180 경계에서 실측으로 큰 점프가
	// 확인되어, 대신 2D 단위벡터를 선형보간 후 재정규화하는 방식으로 교체했다. 벡터 보간에는 애초에
	// "각도 경계"라는 개념이 없어 남/북 등 어떤 방위를 지나가도 항상 연속적으로 움직인다.
	const float LerpAlpha = FMath::Clamp(DeltaTime * CompassYawInterpSpeed, 0.0f, 1.0f);
	const FVector2D LerpedDir = FMath::Lerp(SmoothedYawDir, RawDir, LerpAlpha);
	SmoothedYawDir = LerpedDir.IsNearlyZero() ? RawDir : LerpedDir.GetSafeNormal();
	SmoothedCameraYaw = FMath::RadiansToDegrees(FMath::Atan2(SmoothedYawDir.Y, SmoothedYawDir.X));
}

void URetrieveCompassWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	UpdateSmoothedCameraYaw(GetOwningPlayer(), InDeltaTime);
	UpdateWidgetMarkers(MyGeometry);

	// NativePaint는 Slate의 invalidation 캐싱 대상이라, 카메라가 계속 도는데도 다른 무언가가
	// 레이아웃을 무효화해주기 전까지는 이전 프레임 상태로 멈춰 있다가 캐시가 갱신되는 순간
	// 방위 표시가 한꺼번에 확 점프하는 것처럼 보였다. 매 프레임 강제로 다시 그리게 한다.
	Invalidate(EInvalidateWidgetReason::Paint);
}

void URetrieveCompassWidget::NativeDestruct()
{
	MarkerWidgetPool.Empty();
	EnemyIconSlots.Empty();
	NextEnemySlot = 0;
	Super::NativeDestruct();
}


// ── NativePaint ──────────────────────────────────────────────────────────────

int32 URetrieveCompassWidget::NativePaint(
	const FPaintArgs&         Args,
	const FGeometry&          AllottedGeometry,
	const FSlateRect&         MyCullingRect,
	FSlateWindowElementList&  OutDrawElements,
	int32                     LayerId,
	const FWidgetStyle&       InWidgetStyle,
	bool                      bParentEnabled
) const
{
	int32 CurrentLayer = LayerId;

	const FVector2D Size = AllottedGeometry.GetLocalSize();
	const float Width = Size.X;
	const float Height = Size.Y;
	if (Width < 2.0f || Height < 2.0f)
	{
		return Super::NativePaint(
			Args, AllottedGeometry, MyCullingRect, OutDrawElements,
			CurrentLayer, InWidgetStyle, bParentEnabled
		);
	}

	const float CenterX = Width * 0.5f;
	const float BandCenterY = Height * FMath::Clamp(CompassBandYRatio, 0.0f, 1.0f);

	const APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return Super::NativePaint(
			Args, AllottedGeometry, MyCullingRect, OutDrawElements,
			CurrentLayer, InWidgetStyle, bParentEnabled
		);
	}

	const APawn* Pawn = PC->GetPawn();
	if (!Pawn)
	{
		return Super::NativePaint(
			Args, AllottedGeometry, MyCullingRect, OutDrawElements,
			CurrentLayer, InWidgetStyle, bParentEnabled
		);
	}

	const FVector PlayerLoc = Pawn->GetActorLocation();

	// NativeTick에서 매 프레임 보간된 값 사용 — 카메라를 빠르게 돌려도 띠가 순간이동하듯
	// 튀지 않고 부드럽게 따라가게 함 (CompassYawInterpSpeed로 속도 조절 가능).
	const float CameraYaw = SmoothedCameraYaw;

	// 나침반 띠 경계 밖으로 방위 텍스트/아이콘이 새어나가는 것을 방지.
	// (BearingToX가 범위 밖 항목의 위치 계산은 걸러내지만, 그려지는 요소 자체의
	//  클리핑은 별도로 설정해야 함 — 없으면 카메라 회전 시 텍스트가 위젯 밖에 노출됨)
	OutDrawElements.PushClip(FSlateClippingZone(AllottedGeometry));

	// 배경
	if (bDrawBuiltinBackground)
	{
		if (CompassBandTexture)
		{
			FSlateBrush BandBrush;
			BandBrush.SetResourceObject(CompassBandTexture);
			BandBrush.ImageSize = Size;

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				++CurrentLayer,
				AllottedGeometry.ToPaintGeometry(),
				&BandBrush
			);
		}
		else
		{
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				++CurrentLayer,
				AllottedGeometry.ToPaintGeometry(),
				FCoreStyle::Get().GetBrush("WhiteBrush"),
				ESlateDrawEffect::None,
				BackgroundColor
			);
		}
	}

	// 방위 눈금 + 레이블
	if (bDrawBuiltinCardinals)
	{
		const float UIScale = URetrieveUISettingsLibrary::GetUIScale();
		const FSlateFontInfo CardinalFont = FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt(CardinalFontSize * UIScale));
		const FSlateFontInfo SubCardinalFont = FCoreStyle::GetDefaultFontStyle("Regular", FMath::RoundToInt(SubCardinalFontSize * UIScale));

		for (const FCompassDirection& Dir : GDirections)
		{
			const float X = BearingToX(Dir.Bearing, CameraYaw, Width);
			if (X < 0.0f) { continue; }

			const FLinearColor TickCol = Dir.bCardinal ? CardinalColor : TickColor;
			const float TickH = Dir.bCardinal ? BandCenterY * 1.1f : BandCenterY * 0.7f;
			const float TickW = Dir.bCardinal ? 2.0f : 1.0f;

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				++CurrentLayer,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(TickW, TickH),
					FSlateLayoutTransform(FVector2f(X - TickW * 0.5f, 0.0f))
				),
				FCoreStyle::Get().GetBrush("WhiteBrush"),
				ESlateDrawEffect::None,
				TickCol
			);

			const FSlateFontInfo& UseFont = Dir.bCardinal ? CardinalFont : SubCardinalFont;

			DrawCompassText(
				OutDrawElements,
				CurrentLayer,
				AllottedGeometry,
				Dir.Label,
				FVector2D(X, TickH + (Dir.bCardinal ? 10.0f : 8.0f)),
				UseFont,
				Dir.bCardinal ? CardinalColor : TickColor
			);
		}
	}

	UWorld* World = GetWorld();
	URetrieveMapSubsystem* MapSub = World ? World->GetSubsystem<URetrieveMapSubsystem>() : nullptr;

	// 정적 DA(WorldMapIconData)는 RefreshFromLevel로 라이브 액터 위치를 그대로 굽기 때문에,
	// 그 액터가 로드돼 있으면 정적/라이브 두 경로가 같은 자리에 이중으로 그린다.
	// 특히 오버라이드 아이콘은 정적=레지스트리 기본, 라이브=오버라이드로 서로 달라 두 개로 보였음.
	// → 라이브 아이콘이 근처에 있으면 정적 엔트리를 건너뛴다(라이브 경로 전담).
	TArray<TPair<ERetrieveMapIconType, FVector>> LiveIconSpots;
	if (MapSub)
	{
		for (const URetrieveMapIconComponent* Icon : MapSub->GetIcons())
		{
			if (IsValid(Icon) && IsValid(Icon->GetOwner()) && Icon->bShowOnMinimap)
			{
				LiveIconSpots.Emplace(Icon->IconType, Icon->GetOwner()->GetActorLocation());
			}
		}
	}
	auto HasLiveIconNear = [&LiveIconSpots](ERetrieveMapIconType Type, const FVector& Loc)
	{
		for (const TPair<ERetrieveMapIconType, FVector>& Spot : LiveIconSpots)
		{
			if (Spot.Key == Type && FVector::DistSquared2D(Spot.Value, Loc) < FMath::Square(100.0f))
			{
				return true;
			}
		}
		return false;
	};

	// 월드맵 등록 아이콘
	if (WorldMapIconData && IconRegistry)
	{
		for (const FRetrieveMapIconEntry& Entry : WorldMapIconData->Icons)
		{
			if (Entry.IconType == ERetrieveMapIconType::None ||
				Entry.IconType == ERetrieveMapIconType::Player ||
				EnemyIconTypes.Contains(Entry.IconType) ||
				HiddenIconTypesOnCompass.Contains(Entry.IconType))
			{
				continue;
			}

			if (ShouldUseWidgetMarker(Entry.IconType))
			{
				continue;
			}

			if (CompassIconViewRadius > 0.0f &&
				FVector::Dist2D(PlayerLoc, Entry.WorldLocation) > CompassIconViewRadius)
			{
				continue;
			}

			const FVector Delta = Entry.WorldLocation - PlayerLoc;
			const float BearingDeg = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));

			const float IconX = BearingToX(BearingDeg, CameraYaw, Width);
			if (IconX < 0.0f) { continue; }

			if (HasLiveIconNear(Entry.IconType, Entry.WorldLocation)) { continue; }

			const FRetrieveMapIconRow& Row = IconRegistry->FindRow(Entry.IconType);

			// 정적 엔트리에 구운 개별 오버라이드 반영(월드맵 DrawWorldIcon과 동일 규칙)
			UTexture2D* IconTexture =
				(Entry.bOverrideIcon && Entry.OverrideTexture) ? Entry.OverrideTexture.Get() : Row.IconTexture.Get();
			const FLinearColor IconColor = Entry.bOverrideIcon ? Entry.OverrideColor : Row.IconColor;
			const float IconSize = FMath::Max(
				(Entry.bOverrideIcon ? Entry.OverrideSize : Row.IconSize) * CompassIconSizeScale, 8.0f);

			const FVector2D IconSz(IconSize, IconSize);
			const FVector2D DrawPos(
				IconX - IconSize * 0.5f,
				BandCenterY - IconSize * 0.5f - 10.0f
			);

			FSlateBrush IconBrush;
			if (IconTexture)
			{
				IconBrush.SetResourceObject(IconTexture);
			}
			IconBrush.ImageSize = IconSz;

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				++CurrentLayer,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(IconSz),
					FSlateLayoutTransform(FVector2f(DrawPos))
				),
				IconTexture ? &IconBrush : FCoreStyle::Get().GetBrush("WhiteBrush"),
				ESlateDrawEffect::None,
				IconColor
			);
		}
	}

	// 라이브 아이콘(적뿐 아니라 상호작용 대상 — 모닥불/상점/POI/전초기지 등 미니맵과 동일 소스)
	// 중 bOverrideIcon=true로 등록해둔 개체. 풀링된 EnemyMarkerWidgetClass는 에너미 전용 고정
	// 위젯이라 개별 텍스처를 못 담으므로, 오버라이드된 개체는 미니맵(DrawIcon)과 동일하게
	// 여기서 직접 텍스처를 그린다 — UpdateWidgetMarkers에서는 이 개체들을 풀링 경로에서 제외한다.
	// 타입 제한 없음(이전에는 EnemyIconTypes로 한정되어 있어 상호작용 대상 오버라이드 아이콘이
	// 나침반에서만 안 보이는 원인이었음).
	if (MapSub)
	{
		for (const URetrieveMapIconComponent* Icon : MapSub->GetIcons())
		{
			if (!IsValid(Icon) || !IsValid(Icon->GetOwner()))      { continue; }
			if (!Icon->bShowOnMinimap)                             { continue; }
			if (!Icon->bOverrideIcon)                              { continue; }
			if (HiddenIconTypesOnCompass.Contains(Icon->IconType)) { continue; }

			const FVector IconWorld = Icon->GetOwner()->GetActorLocation();
			const float ViewRadius = EnemyIconTypes.Contains(Icon->IconType) ? EnemyViewRadius : CompassIconViewRadius;

			if (ViewRadius > 0.0f &&
				FVector::Dist2D(PlayerLoc, IconWorld) > ViewRadius)
			{
				continue;
			}

			const FVector Delta = IconWorld - PlayerLoc;
			const float BearingDeg = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
			const float IconX = BearingToX(BearingDeg, CameraYaw, Width);
			if (IconX < 0.0f) { continue; }

			const float IconSize = FMath::Max(Icon->OverrideSize * CompassIconSizeScale, 8.0f);
			const FVector2D IconSz(IconSize, IconSize);
			const FVector2D DrawPos(
				IconX + WidgetMarkerOffset.X - IconSize * 0.5f,
				BandCenterY + WidgetMarkerOffset.Y - IconSize * 0.5f
			);

			FSlateBrush IconBrush;
			if (Icon->OverrideTexture)
			{
				IconBrush.SetResourceObject(Icon->OverrideTexture);
			}
			IconBrush.ImageSize = IconSz;

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				++CurrentLayer,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(IconSz),
					FSlateLayoutTransform(FVector2f(DrawPos))
				),
				Icon->OverrideTexture ? &IconBrush : FCoreStyle::Get().GetBrush("WhiteBrush"),
				ESlateDrawEffect::None,
				Icon->OverrideColor
			);
		}
	}

	// 라이브 상호작용 대상 아이콘(모닥불/상점/POI/전초기지 등, Enemy/Boss 아님, 오버라이드도 아님).
	// 전용 풀링 위젯이 없으므로 IconRegistry 룩업으로 정적 아이콘과 동일하게 직접 그린다.
	// (이전에는 라이브 GetIcons() 경로가 EnemyIconTypes로만 한정되어 있어 이 부류가 나침반에서
	//  통째로 누락됐었음 — 미니맵/월드맵은 타입 제한 없이 다 그려서 거기서는 문제가 없었음)
	if (MapSub && IconRegistry)
	{
		for (const URetrieveMapIconComponent* Icon : MapSub->GetIcons())
		{
			if (!IsValid(Icon) || !IsValid(Icon->GetOwner()))      { continue; }
			if (!Icon->bShowOnMinimap)                             { continue; }
			if (Icon->bOverrideIcon)                               { continue; } // 위 블록이 전담
			if (EnemyIconTypes.Contains(Icon->IconType))           { continue; } // 에너미는 풀링 위젯 전담
			if (HiddenIconTypesOnCompass.Contains(Icon->IconType)) { continue; }

			const FVector IconWorld = Icon->GetOwner()->GetActorLocation();

			if (CompassIconViewRadius > 0.0f &&
				FVector::Dist2D(PlayerLoc, IconWorld) > CompassIconViewRadius)
			{
				continue;
			}

			const FVector Delta = IconWorld - PlayerLoc;
			const float BearingDeg = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
			const float IconX = BearingToX(BearingDeg, CameraYaw, Width);
			if (IconX < 0.0f) { continue; }

			const FRetrieveMapIconRow& Row = IconRegistry->FindRow(Icon->IconType);
			UTexture2D* IconTexture = Row.IconTexture;
			const float IconSize = FMath::Max(Row.IconSize * CompassIconSizeScale, 8.0f);
			const FVector2D IconSz(IconSize, IconSize);
			const FVector2D DrawPos(
				IconX + WidgetMarkerOffset.X - IconSize * 0.5f,
				BandCenterY + WidgetMarkerOffset.Y - IconSize * 0.5f
			);

			FSlateBrush IconBrush;
			if (IconTexture)
			{
				IconBrush.SetResourceObject(IconTexture);
			}
			IconBrush.ImageSize = IconSz;

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				++CurrentLayer,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(IconSz),
					FSlateLayoutTransform(FVector2f(DrawPos))
				),
				IconTexture ? &IconBrush : FCoreStyle::Get().GetBrush("WhiteBrush"),
				ESlateDrawEffect::None,
				Row.IconColor
			);
		}
	}

	if (MapSub)
	{
		const TArray<FUserWaypoint>& Waypoints = MapSub->GetUserWaypoints();
		const FSlateFontInfo WpFont = FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt(9 * URetrieveUISettingsLibrary::GetUIScale()));

		for (const FUserWaypoint& WP : Waypoints)
		{
			const FVector Delta = WP.WorldLocation - PlayerLoc;
			const float DistXY = FVector2D(Delta.X, Delta.Y).Size();
			const float BearingDeg = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));

			const float MarkerX = BearingToX(BearingDeg, CameraYaw, Width);
			if (MarkerX < 0.0f) { continue; }

			if (HasUsableMarkerPanel() && UserWaypointMarkerWidgetClass)
			{
				continue;
			}

			const float HalfSz = WaypointMarkerSize * 0.5f;
			const FLinearColor MarkerCol = WP.Color;

			FSlateBrush WpBrush;
			if (WaypointMarkerTexture)
			{
				WpBrush.SetResourceObject(WaypointMarkerTexture);
				WpBrush.ImageSize = FVector2D(WaypointMarkerSize, WaypointMarkerSize);
			}

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				++CurrentLayer,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(WaypointMarkerSize, WaypointMarkerSize),
					FSlateLayoutTransform(FVector2f(MarkerX - HalfSz, BandCenterY - HalfSz))
				),
				WaypointMarkerTexture ? &WpBrush : FCoreStyle::Get().GetBrush("WhiteBrush"),
				ESlateDrawEffect::None,
				MarkerCol
			);

			if (DistXY > 0.1f)
			{
				const FString DistStr = FString::Printf(TEXT("%.0fm"), DistXY / 100.0f);

				DrawCompassText(
					OutDrawElements,
					CurrentLayer,
					AllottedGeometry,
					DistStr,
					FVector2D(MarkerX, BandCenterY + HalfSz + 10.0f),
					WpFont,
					MarkerCol
				);
			}
		}
	}

	// ── 퀘스트 목표 마커 ─────────────────────────────────────────────────────
	// 화면 마커가 거리/개수 제한으로 내려간 목표도 방향만은 여기서 계속 알려준다.
	// (AcquireItem처럼 화면 마커를 만들지 않는 목표는 나침반이 유일한 방향 단서)
	if (const URetrieveObjectiveMarkerSubsystem* MarkerSub =
		GetWorld() ? GetWorld()->GetSubsystem<URetrieveObjectiveMarkerSubsystem>() : nullptr)
	{
		const FSlateFontInfo ObjFont = FCoreStyle::GetDefaultFontStyle(
			"Bold", FMath::RoundToInt(9 * URetrieveUISettingsLibrary::GetUIScale()));

		const float ObjectiveDiscoveryRadius = MarkerSub->GetDiscoveryRadius();

		for (const FRetrieveObjectiveMarker& Marker : MarkerSub->GetMarkers())
		{
			if (!Marker.State.bVisible)
			{
				continue;
			}
			// 화면 마커와 같은 발견 반경(메인은 예외).
			if (!URetrieveObjectiveMarkerSubsystem::PassesDiscoveryRadius(
					Marker, PlayerLoc, ObjectiveDiscoveryRadius))
			{
				continue;
			}

			const FVector Delta = Marker.State.WorldLocation - PlayerLoc;
			const float DistXY = FVector2D(Delta.X, Delta.Y).Size();
			const float BearingDeg = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));

			const float MarkerX = BearingToX(BearingDeg, CameraYaw, Width);
			if (MarkerX < 0.0f)
			{
				continue;
			}

			FLinearColor MarkerCol = ObjectiveMarkerColorSide;
			if (Marker.Kind == ERetrieveObjectiveMarkerKind::Main)
			{
				MarkerCol = ObjectiveMarkerColorMain;
			}
			else if (Marker.Kind == ERetrieveObjectiveMarkerKind::TurnIn)
			{
				MarkerCol = ObjectiveMarkerColorTurnIn;
			}
			else if (Marker.Kind == ERetrieveObjectiveMarkerKind::Offer)
			{
				MarkerCol = ObjectiveMarkerColorOffer;
			}

			const float HalfSz = ObjectiveMarkerSize * 0.5f;

			FSlateBrush ObjBrush;
			if (WaypointMarkerTexture)
			{
				ObjBrush.SetResourceObject(WaypointMarkerTexture);
				ObjBrush.ImageSize = FVector2D(ObjectiveMarkerSize, ObjectiveMarkerSize);
			}

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				++CurrentLayer,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(ObjectiveMarkerSize, ObjectiveMarkerSize),
					FSlateLayoutTransform(FVector2f(MarkerX - HalfSz, BandCenterY - HalfSz))
				),
				WaypointMarkerTexture ? &ObjBrush : FCoreStyle::Get().GetBrush("WhiteBrush"),
				ESlateDrawEffect::None,
				MarkerCol
			);

			if (DistXY > 0.1f)
			{
				DrawCompassText(
					OutDrawElements,
					CurrentLayer,
					AllottedGeometry,
					FString::Printf(TEXT("%.0fm"), DistXY / 100.0f),
					FVector2D(MarkerX, BandCenterY + HalfSz + 10.0f),
					ObjFont,
					MarkerCol
				);
			}
		}
	}

	// 중앙 지시선
	const float LineW = 2.0f;
	const float LineH = BandCenterY * 1.3f;

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		++CurrentLayer,
		AllottedGeometry.ToPaintGeometry(
			FVector2f(LineW, LineH),
			FSlateLayoutTransform(FVector2f(CenterX - LineW * 0.5f, 0.0f))
		),
		FCoreStyle::Get().GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		CenterTickColor
	);

	OutDrawElements.PopClip();

	CurrentLayer = Super::NativePaint(
		Args,
		AllottedGeometry,
		MyCullingRect,
		OutDrawElements,
		CurrentLayer,
		InWidgetStyle,
		bParentEnabled
	);

	return CurrentLayer;
}
