#include "UI/Map/RetrieveWorldMapWidget.h"
#include "Subsystems/RetrieveMapSubsystem.h"
#include "Components/RetrieveMapIconComponent.h"
#include "Data/RetrieveMapIconRegistry.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"

// ---------- 초기화 ----------

void URetrieveWorldMapWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetVisibility(ESlateVisibility::Visible);
	SetClipping(EWidgetClipping::ClipToBoundsAlways);
	ZoomLevel    = MinZoom;
	ViewCenterUV = FVector2D(0.5f, 0.5f);
}

// ---------- 공개 함수 ----------

void URetrieveWorldMapWidget::CenterOnPlayer()
{
	const APlayerController* PC = GetOwningPlayer();
	if (!PC || !PC->GetPawn()) { return; }

	UWorld* World = GetWorld();
	if (!World) { return; }

	URetrieveMapSubsystem* MapSub = World->GetSubsystem<URetrieveMapSubsystem>();
	if (!MapSub || !MapSub->HasValidBounds()) { return; }

	// WorldToUV: 축별 실제 범위로 나눈 정확한 UV
	ViewCenterUV = MapSub->WorldToUV(PC->GetPawn()->GetActorLocation());
	ClampViewCenter();
}

void URetrieveWorldMapWidget::ZoomIn()
{
	ZoomLevel = FMath::Clamp(ZoomLevel + ZoomStep, MinZoom, MaxZoom);
	ClampViewCenter();
}

void URetrieveWorldMapWidget::ZoomOut()
{
	ZoomLevel = FMath::Clamp(ZoomLevel - ZoomStep, MinZoom, MaxZoom);
	ClampViewCenter();
}

// ---------- 헬퍼 — 종횡비 계산 ----------

void URetrieveWorldMapWidget::ComputeBaseMapSize(
	const URetrieveMapSubsystem* MapSub,
	const FVector2D& MapViewSize,
	float& OutBaseW, float& OutBaseH) const
{
	if (bStretchMapToViewport)
	{
		OutBaseW = FMath::Max(MapViewSize.X, 1.0f);
		OutBaseH = FMath::Max(MapViewSize.Y, 1.0f);
		return;
	}

	const float MapSide = FMath::Min(MapViewSize.X, MapViewSize.Y);

	// 기본값: 정사각
	OutBaseW = OutBaseH = MapSide;

	if (!MapSub || !MapSub->HasValidBounds()) { return; }

	// TextureAspect = U축(Y world, 화면 가로) / V축(X world, 화면 세로)
	const float ExtentY = FMath::Max(MapSub->MapExtentXY.Y, 1.0f);  // U 방향
	const float ExtentX = FMath::Max(MapSub->MapExtentXY.X, 1.0f);  // V 방향
	const float Aspect  = ExtentY / ExtentX;                         // W:H 비율

	if (Aspect >= 1.0f)
	{
		// 가로가 더 긴 레벨: 가로를 MapSide에 맞추고 세로를 줄임
		OutBaseW = MapSide;
		OutBaseH = MapSide / Aspect;
	}
	else
	{
		// 세로가 더 긴 레벨: 세로를 MapSide에 맞추고 가로를 줄임
		OutBaseW = MapSide * Aspect;
		OutBaseH = MapSide;
	}
}

// ---------- 페인트 ----------

int32 URetrieveWorldMapWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled
) const
{
	int32 CurrentLayer = Super::NativePaint(
		Args, AllottedGeometry, MyCullingRect, OutDrawElements,
		LayerId, InWidgetStyle, bParentEnabled
	);

	const FVector2D WidgetSize = AllottedGeometry.GetLocalSize();
	if (WidgetSize.X <= 1.0f || WidgetSize.Y <= 1.0f) { return CurrentLayer; }

	FVector2D MapViewTopLeft;
	FVector2D MapViewSize;
	GetMapViewRect(AllottedGeometry, MapViewTopLeft, MapViewSize);

	const FVector2D Center  = MapViewTopLeft + MapViewSize * 0.5f;

	// 게임 상태 취득
	const APlayerController* PC    = GetOwningPlayer();
	UWorld*                  World = GetWorld();
	URetrieveMapSubsystem* MapSub = World ? World->GetSubsystem<URetrieveMapSubsystem>() : nullptr;

	// ── 레벨 종횡비를 반영한 맵 표시 크기 계산 ──────────────────────────────
	// BaseW/BaseH: ZoomLevel=1 기준, MapSide 안에 레벨 비율대로 최대 채움
	// ScaledW/H  : 실제 그릴 픽셀 크기 (줌 적용)
	float BaseW = MapViewSize.X, BaseH = MapViewSize.Y;
	ComputeBaseMapSize(MapSub, MapViewSize, BaseW, BaseH);

	const float     ScaledW = BaseW * ZoomLevel;
	const float     ScaledH = BaseH * ZoomLevel;
	const FVector2D MapTopLeft(
		Center.X - ScaledW * ViewCenterUV.X,
		Center.Y - ScaledH * ViewCenterUV.Y
	);
	const FVector2D MapDrawSize(ScaledW, ScaledH);

	// 클리핑
	const FVector2D AbsMapTopLeft = FVector2D(AllottedGeometry.LocalToAbsolute(FVector2f(MapViewTopLeft)));
	const FVector2D AbsMapBottomRight = FVector2D(AllottedGeometry.LocalToAbsolute(FVector2f(MapViewTopLeft + MapViewSize)));
	const FSlateClippingZone ClipZone(FSlateRect(AbsMapTopLeft.X, AbsMapTopLeft.Y, AbsMapBottomRight.X, AbsMapBottomRight.Y));
	OutDrawElements.PushClip(ClipZone);

	// ── 맵 텍스처 (단일 텍스처 or 타일) ────────────────────────────────────────
	if (MapSub && MapSub->HasTiles())
	{
		// ── 타일 모드: 현재 뷰포트와 겹치는 타일만 렌더링 ──────────────────
		// 현재 화면에 보이는 UV 범위 계산
		const FVector2D VisUVMin(
			ViewCenterUV.X - 0.5f * MapViewSize.X / ScaledW,
			ViewCenterUV.Y - 0.5f * MapViewSize.Y / ScaledH
		);
		const FVector2D VisUVMax(
			ViewCenterUV.X + 0.5f * MapViewSize.X / ScaledW,
			ViewCenterUV.Y + 0.5f * MapViewSize.Y / ScaledH
		);

		for (const FRetrieveMapTile& Tile : MapSub->MapTiles)
		{
			if (!Tile.HasTexture() || !Tile.Overlaps(VisUVMin, VisUVMax))
			{
				continue;  // 화면 밖 타일 스킵 (컬링)
			}

			// 타일 UV 범위 → 화면 픽셀 위치
			const FVector2D TileTopLeft = UVToScreen(Tile.UVMin, Center, ScaledW, ScaledH);
			const FVector2D TileDrawSz(
				(Tile.UVMax.X - Tile.UVMin.X) * ScaledW,
				(Tile.UVMax.Y - Tile.UVMin.Y) * ScaledH
			);

			FSlateBrush TileBrush;
			TileBrush.SetResourceObject(Tile.Texture);
			TileBrush.ImageSize = TileDrawSz;

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				++CurrentLayer,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(TileDrawSz),
					FSlateLayoutTransform(FVector2f(TileTopLeft))
				),
				&TileBrush
			);
		}
	}
	else
	{
		// ── 단일 텍스처 모드 ──────────────────────────────────────────────
		UTexture2D* ActiveTexture = BakedMapTexture;
		if (!ActiveTexture && MapSub && MapSub->BakedMapTexture)
		{
			ActiveTexture = MapSub->BakedMapTexture;
		}

		if (ActiveTexture)
		{
			FSlateBrush MapBrush;
			MapBrush.SetResourceObject(ActiveTexture);
			MapBrush.ImageSize = FVector2D(BaseW, BaseH);

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				++CurrentLayer,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(MapDrawSize),
					FSlateLayoutTransform(FVector2f(MapTopLeft))
				),
				&MapBrush
			);
		}
	}

	if (PC && PC->GetPawn() && MapSub && MapSub->HasValidBounds())
	{
		const FVector   PlayerLoc    = PC->GetPawn()->GetActorLocation();
		const FVector2D PlayerUV     = MapSub->WorldToUV(PlayerLoc);
		const FVector2D PlayerScreen = UVToScreen(PlayerUV, Center, ScaledW, ScaledH);

		const FSlateRect MapRect(MapTopLeft.X, MapTopLeft.Y,
		                         MapTopLeft.X + ScaledW, MapTopLeft.Y + ScaledH);

		// ── 플레이어 마커 + 현재 위치 레이블 ─────────────────────────────────
		if (MapRect.ContainsPoint(FVector2D(PlayerScreen)))
		{
			const float CameraYaw = PC->PlayerCameraManager
				? PC->PlayerCameraManager->GetCameraRotation().Yaw
				: PC->GetControlRotation().Yaw;

			const FVector2D MarkerSz(PlayerMarkerSize, PlayerMarkerSize);
			const FVector2D MarkerTopLeft   = PlayerScreen - MarkerSz * 0.5f;
			const FVector2D AbsPlayerScreen =
				FVector2D(AllottedGeometry.LocalToAbsolute(FVector2f(PlayerScreen)));

			FSlateBrush PlayerBrush;
			PlayerBrush.SetResourceObject(PlayerMarkerTexture);
			PlayerBrush.ImageSize = MarkerSz;

			FSlateDrawElement::MakeRotatedBox(
				OutDrawElements,
				++CurrentLayer,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(MarkerSz),
					FSlateLayoutTransform(FVector2f(MarkerTopLeft))
				),
				PlayerMarkerTexture ? &PlayerBrush : FCoreStyle::Get().GetBrush("WhiteBrush"),
				ESlateDrawEffect::None,
				FMath::DegreesToRadians(CameraYaw),
				TOptional<FVector2D>(AbsPlayerScreen),
				FSlateDrawElement::ERotationSpace::RelativeToWorld,
				PlayerMarkerColor
			);

			if (!CurrentLocationText.IsEmpty())
			{
				const FSlateFontInfo LabelFont =
					FCoreStyle::GetDefaultFontStyle("Bold", PlayerLabelFontSize);
				const FVector2D LabelAnchor(
					PlayerScreen.X,
					PlayerScreen.Y - MarkerSz.Y * 0.5f - 6.0f
				);
				DrawLabel(OutDrawElements, CurrentLayer, AllottedGeometry,
				          CurrentLocationText.ToString(), LabelAnchor, LabelFont, LabelColor);
			}
		}

		// ── 등록 아이콘 ────────────────────────────────────────────────────────
		for (const URetrieveMapIconComponent* Icon : MapSub->GetIcons())
		{
			if (!IsValid(Icon) || !IsValid(Icon->GetOwner()) || !Icon->bShowOnMinimap) { continue; }

			const FVector2D IconUV     = MapSub->WorldToUV(Icon->GetOwner()->GetActorLocation());
			const FVector2D IconScreen = UVToScreen(IconUV, Center, ScaledW, ScaledH);

			if (!MapRect.ContainsPoint(FVector2D(IconScreen))) { continue; }

			DrawWorldIcon(OutDrawElements, CurrentLayer, AllottedGeometry, Icon, IconScreen);

			if (!Icon->MapLabel.IsEmpty() && Icon->bShowLabelOnWorldMap)
			{
				float IconHalfSize = 8.0f;
				if (Icon->bOverrideIcon)
				{
					IconHalfSize = Icon->OverrideSize * 0.5f;
				}
				else if (IconRegistry)
				{
					IconHalfSize = IconRegistry->FindRow(Icon->IconType).IconSize * 0.5f;
				}

				const FSlateFontInfo IconFont =
					FCoreStyle::GetDefaultFontStyle("Regular", IconLabelFontSize);
				const FVector2D LabelAnchor(
					IconScreen.X,
					IconScreen.Y + IconHalfSize + 4.0f
				);
				DrawLabel(OutDrawElements, CurrentLayer, AllottedGeometry,
				          Icon->MapLabel.ToString(), LabelAnchor, IconFont, LabelColor);
			}
		}
	}

	OutDrawElements.PopClip();
	return CurrentLayer;
}

// ---------- 마우스 이벤트 ----------

FReply URetrieveWorldMapWidget::NativeOnMouseWheel(
	const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	const FVector2D WidgetSize    = InGeometry.GetLocalSize();
	const FVector2D LocalMousePos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

	FVector2D MapViewTopLeft;
	FVector2D MapViewSize;
	GetMapViewRect(InGeometry, MapViewTopLeft, MapViewSize);
	if (!IsInsideMapView(LocalMousePos, MapViewTopLeft, MapViewSize))
	{
		return FReply::Unhandled();
	}

	const FVector2D Center        = MapViewTopLeft + MapViewSize * 0.5f;

	URetrieveMapSubsystem* MapSub = GetWorld()
		? GetWorld()->GetSubsystem<URetrieveMapSubsystem>() : nullptr;

	float BaseW = MapViewSize.X, BaseH = MapViewSize.Y;
	ComputeBaseMapSize(MapSub, MapViewSize, BaseW, BaseH);

	const float OldZoom = ZoomLevel;
	ZoomLevel = FMath::Clamp(
		ZoomLevel + InMouseEvent.GetWheelDelta() * ZoomStep, MinZoom, MaxZoom
	);

	if (!FMath::IsNearlyEqual(OldZoom, ZoomLevel))
	{
		// 커서 아래 UV 고정 줌 — X/Y 축별 독립 스케일 적용
		const FVector2D DeltaFromCenter = LocalMousePos - Center;
		const FVector2D MouseUV(
			ViewCenterUV.X + DeltaFromCenter.X / (BaseW * OldZoom),
			ViewCenterUV.Y + DeltaFromCenter.Y / (BaseH * OldZoom)
		);
		ViewCenterUV.X = MouseUV.X - DeltaFromCenter.X / (BaseW * ZoomLevel);
		ViewCenterUV.Y = MouseUV.Y - DeltaFromCenter.Y / (BaseH * ZoomLevel);
		ClampViewCenter();
	}

	return FReply::Handled();
}

FReply URetrieveWorldMapWidget::NativeOnMouseButtonDown(
	const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		const FVector2D WidgetSize = InGeometry.GetLocalSize();
		const FVector2D LocalMousePos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());
		FVector2D MapViewTopLeft;
		FVector2D MapViewSize;
		GetMapViewRect(InGeometry, MapViewTopLeft, MapViewSize);
		if (!IsInsideMapView(LocalMousePos, MapViewTopLeft, MapViewSize))
		{
			return FReply::Unhandled();
		}

		bIsPanning       = true;
		PanStartLocalPos = LocalMousePos;
		PanStartCenterUV = ViewCenterUV;

		if (TSharedPtr<SWidget> PinnedWidget = GetCachedWidget())
		{
			return FReply::Handled().CaptureMouse(PinnedWidget.ToSharedRef());
		}
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FReply URetrieveWorldMapWidget::NativeOnMouseButtonUp(
	const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bIsPanning)
	{
		bIsPanning = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Unhandled();
}

FReply URetrieveWorldMapWidget::NativeOnMouseMove(
	const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bIsPanning) { return FReply::Unhandled(); }

	const FVector2D WidgetSize      = InGeometry.GetLocalSize();
	const FVector2D CurrentLocalPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

	FVector2D MapViewTopLeft;
	FVector2D MapViewSize;
	GetMapViewRect(InGeometry, MapViewTopLeft, MapViewSize);

	URetrieveMapSubsystem* MapSub = GetWorld()
		? GetWorld()->GetSubsystem<URetrieveMapSubsystem>() : nullptr;

	float BaseW = MapViewSize.X, BaseH = MapViewSize.Y;
	ComputeBaseMapSize(MapSub, MapViewSize, BaseW, BaseH);

	// 드래그 방향 반대로 뷰 이동 — X/Y 축별 독립 스케일
	const FVector2D DeltaScreen = CurrentLocalPos - PanStartLocalPos;
	ViewCenterUV.X = PanStartCenterUV.X - DeltaScreen.X / (BaseW * ZoomLevel);
	ViewCenterUV.Y = PanStartCenterUV.Y - DeltaScreen.Y / (BaseH * ZoomLevel);
	ClampViewCenter();

	return FReply::Handled();
}

// ---------- 헬퍼 ----------

void URetrieveWorldMapWidget::GetMapViewRect(const FGeometry& AllottedGeometry, FVector2D& OutTopLeft, FVector2D& OutSize) const
{
	const FVector2D WidgetSize = AllottedGeometry.GetLocalSize();

	const UWidget* ViewportWidget = MapViewport ? MapViewport.Get() : HorizontalBox_Body.Get();
	if (ViewportWidget)
	{
		const FGeometry& ViewGeometry = ViewportWidget->GetCachedGeometry();
		const FVector2D ViewSize = ViewGeometry.GetLocalSize();
		if (ViewSize.X > 1.0f && ViewSize.Y > 1.0f)
		{
			const FVector2D AbsTopLeft = FVector2D(ViewGeometry.LocalToAbsolute(FVector2f::ZeroVector));
			const FVector2D AbsBottomRight = FVector2D(ViewGeometry.LocalToAbsolute(FVector2f(ViewSize)));
			OutTopLeft = AllottedGeometry.AbsoluteToLocal(AbsTopLeft);
			const FVector2D BottomRight = AllottedGeometry.AbsoluteToLocal(AbsBottomRight);
			OutSize = FVector2D(
				FMath::Max(1.0f, BottomRight.X - OutTopLeft.X),
				FMath::Max(1.0f, BottomRight.Y - OutTopLeft.Y)
			);
			return;
		}
	}

	const float Left = FMath::Clamp(MapViewportPadding.Left, 0.0f, WidgetSize.X);
	const float Top = FMath::Clamp(MapViewportPadding.Top, 0.0f, WidgetSize.Y);
	const float Right = FMath::Clamp(MapViewportPadding.Right, 0.0f, FMath::Max(0.0f, WidgetSize.X - Left));
	const float Bottom = FMath::Clamp(MapViewportPadding.Bottom, 0.0f, FMath::Max(0.0f, WidgetSize.Y - Top));

	OutTopLeft = FVector2D(Left, Top);
	OutSize = FVector2D(
		FMath::Max(1.0f, WidgetSize.X - Left - Right),
		FMath::Max(1.0f, WidgetSize.Y - Top - Bottom)
	);
}

bool URetrieveWorldMapWidget::IsInsideMapView(const FVector2D& LocalPosition, const FVector2D& MapTopLeft, const FVector2D& MapSize) const
{
	return LocalPosition.X >= MapTopLeft.X
		&& LocalPosition.Y >= MapTopLeft.Y
		&& LocalPosition.X <= MapTopLeft.X + MapSize.X
		&& LocalPosition.Y <= MapTopLeft.Y + MapSize.Y;
}

void URetrieveWorldMapWidget::ClampViewCenter()
{
	const float HalfExtent = 0.5f / ZoomLevel;
	ViewCenterUV.X = FMath::Clamp(ViewCenterUV.X, HalfExtent, 1.0f - HalfExtent);
	ViewCenterUV.Y = FMath::Clamp(ViewCenterUV.Y, HalfExtent, 1.0f - HalfExtent);
}

FVector2D URetrieveWorldMapWidget::UVToScreen(
	const FVector2D& UV, const FVector2D& Center,
	float ScaledW, float ScaledH) const
{
	// UV=ViewCenterUV → Center, 축별 스케일로 오프셋 계산
	return Center + FVector2D(
		(UV.X - ViewCenterUV.X) * ScaledW,
		(UV.Y - ViewCenterUV.Y) * ScaledH
	);
}

void URetrieveWorldMapWidget::DrawWorldIcon(
	FSlateWindowElementList& OutDrawElements,
	int32& LayerId,
	const FGeometry& AllottedGeometry,
	const URetrieveMapIconComponent* Icon,
	const FVector2D& ScreenPos
) const
{
	UTexture2D*  Texture = nullptr;
	FLinearColor Color   = FLinearColor::White;
	float        Size    = 16.0f;

	if (Icon->bOverrideIcon)
	{
		Texture = Icon->OverrideTexture;
		Color   = Icon->OverrideColor;
		Size    = Icon->OverrideSize;
	}
	else if (IconRegistry)
	{
		const FRetrieveMapIconRow& Row = IconRegistry->FindRow(Icon->IconType);
		Texture = Row.IconTexture;
		Color   = Row.IconColor;
		Size    = Row.IconSize;
	}

	const FVector2D IconSz(Size, Size);
	const FVector2D DrawPos = ScreenPos - IconSz * 0.5f;

	FSlateBrush IconBrush;
	IconBrush.SetResourceObject(Texture);
	IconBrush.ImageSize = IconSz;

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		++LayerId,
		AllottedGeometry.ToPaintGeometry(
			FVector2f(IconSz),
			FSlateLayoutTransform(FVector2f(DrawPos))
		),
		Texture ? &IconBrush : FCoreStyle::Get().GetBrush("WhiteBrush"),
		ESlateDrawEffect::None,
		Color
	);
}

void URetrieveWorldMapWidget::DrawLabel(
	FSlateWindowElementList& OutDrawElements,
	int32& LayerId,
	const FGeometry& AllottedGeometry,
	const FString& Text,
	const FVector2D& CenterPos,
	const FSlateFontInfo& FontInfo,
	const FLinearColor& Color
) const
{
	if (Text.IsEmpty()) { return; }

	const TSharedRef<FSlateFontMeasure> FontMeasure =
		FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	const FVector2D TextSize = FontMeasure->Measure(Text, FontInfo);

	const FVector2D TextTopLeft(CenterPos.X - TextSize.X * 0.5f, CenterPos.Y);
	const FVector2D ShadowTopLeft = TextTopLeft + FVector2D(1.0f, 1.0f);

	FSlateDrawElement::MakeText(
		OutDrawElements, ++LayerId,
		AllottedGeometry.ToPaintGeometry(FVector2f(TextSize), FSlateLayoutTransform(FVector2f(ShadowTopLeft))),
		Text, FontInfo, ESlateDrawEffect::None, LabelShadowColor
	);
	FSlateDrawElement::MakeText(
		OutDrawElements, ++LayerId,
		AllottedGeometry.ToPaintGeometry(FVector2f(TextSize), FSlateLayoutTransform(FVector2f(TextTopLeft))),
		Text, FontInfo, ESlateDrawEffect::None, Color
	);
}
