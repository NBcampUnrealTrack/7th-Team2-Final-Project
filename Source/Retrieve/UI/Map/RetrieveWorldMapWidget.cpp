#include "UI/Map/RetrieveWorldMapWidget.h"
#include "Subsystems/RetrieveMapSubsystem.h"
#include "Components/RetrieveMapIconComponent.h"
#include "Data/RetrieveMapIconRegistry.h"
#include "World/RetrieveBonfireActor.h"
#include "Save/RetrieveSaveSubsystem.h"
#include "Player/RetrievePlayerController.h"

#include "Camera/PlayerCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "HAL/PlatformTime.h"

// ---------- 초기화 ----------

URetrieveWorldMapWidget::URetrieveWorldMapWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FastTravelDialogClass = TSoftClassPtr<UUserWidget>(
		FSoftObjectPath(TEXT("/Game/Retrieve/UI/HUD/WBP_FastTravelDialog.WBP_FastTravelDialog_C")));
	FastTravelLoadingClass = TSoftClassPtr<UUserWidget>(
		FSoftObjectPath(TEXT("/Game/Retrieve/UI/HUD/WBP_LoadingCutscene.WBP_LoadingCutscene_C")));
}

void URetrieveWorldMapWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetVisibility(ESlateVisibility::Visible);
	SetClipping(EWidgetClipping::ClipToBoundsAlways);
	ZoomLevel    = MinZoom;
	ViewCenterUV = FVector2D(0.5f, 0.5f);
	bPendingCenterOnPlayer = true;
	bWasLeftMouseButtonDown = false;

	const FString LocationString = CurrentLocationText.ToString();
	if (LocationString.IsEmpty() || LocationString.Contains(TEXT("?")))
	{
		CurrentLocationText = NSLOCTEXT("RetrieveWorldMap", "CurrentLocation", "현재 위치");
	}

	// 월드맵을 열 때마다 현재 로드된 모든 MapIconComponent를 스캔해서 스냅샷 갱신.
	if (UWorld* World = GetWorld())
	{
		if (URetrieveMapSubsystem* MapSub = World->GetSubsystem<URetrieveMapSubsystem>())
		{
			MapSub->RefreshWorldMapSnapshots();
		}
	}
}

// ---------- 공개 함수 ----------

void URetrieveWorldMapWidget::SetIconTypeVisible(ERetrieveMapIconType IconType, bool bVisible)
{
	if (bVisible)
	{
		HiddenIconTypesOnWorldMap.Remove(IconType);
	}
	else
	{
		HiddenIconTypesOnWorldMap.Add(IconType);
	}
}

bool URetrieveWorldMapWidget::IsIconTypeVisible(ERetrieveMapIconType IconType) const
{
	return !HiddenIconTypesOnWorldMap.Contains(IconType);
}

void URetrieveWorldMapWidget::CenterOnPlayer()
{
	const APlayerController* PC = GetWorldMapPlayerController();
	if (!PC || !PC->GetPawn()) { return; }

	UWorld* World = GetWorld();
	if (!World) { return; }

	URetrieveMapSubsystem* MapSub = World->GetSubsystem<URetrieveMapSubsystem>();
	if (!MapSub || !MapSub->HasValidBounds()) { return; }

	// WorldToUV: 축별 실제 범위로 나눈 정확한 UV
	ViewCenterUV = MapSub->WorldToUV(PC->GetPawn()->GetActorLocation());
	ClampViewCenter();
	bPendingCenterOnPlayer = false;
}

void URetrieveWorldMapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (bPendingCenterOnPlayer)
	{
		CenterOnPlayer();
	}

	if (FSlateApplication::IsInitialized())
	{
		const bool bIsLeftMouseButtonDown =
			FSlateApplication::Get().GetPressedMouseButtons().Contains(EKeys::LeftMouseButton);
		const APlayerController* PC = GetWorldMapPlayerController();
		const bool bWasInputKeyJustPressed =
			PC && PC->WasInputKeyJustPressed(EKeys::LeftMouseButton);

		if ((bIsLeftMouseButtonDown && !bWasLeftMouseButtonDown) || bWasInputKeyJustPressed)
		{
			const FVector2D LocalMousePos =
				MyGeometry.AbsoluteToLocal(FSlateApplication::Get().GetCursorPos());
			UE_LOG(LogTemp, Log,
				TEXT("[WorldMap] Mouse press observed - Local=(%.1f, %.1f) Slate=%s PlayerInput=%s"),
				LocalMousePos.X,
				LocalMousePos.Y,
				bIsLeftMouseButtonDown ? TEXT("Down") : TEXT("Up"),
				bWasInputKeyJustPressed ? TEXT("Pressed") : TEXT("Idle"));
			HandleBonfireIconClick(MyGeometry, LocalMousePos);
		}

		bWasLeftMouseButtonDown = bIsLeftMouseButtonDown;
	}
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

	const UTexture2D* ActiveTexture = BakedMapTexture;
	if (!ActiveTexture && MapSub)
	{
		ActiveTexture = MapSub->BakedMapTexture;
	}

	if (ActiveTexture && ActiveTexture->GetSizeX() > 0 && ActiveTexture->GetSizeY() > 0)
	{
		const float TextureAspect = static_cast<float>(ActiveTexture->GetSizeX())
			/ static_cast<float>(ActiveTexture->GetSizeY());
		OutBaseW = FMath::Max(MapViewSize.X, 1.0f);
		OutBaseH = OutBaseW / TextureAspect;
		if (OutBaseH > MapViewSize.Y)
		{
			OutBaseH = FMath::Max(MapViewSize.Y, 1.0f);
			OutBaseW = OutBaseH * TextureAspect;
		}
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
	const APlayerController* PC    = GetWorldMapPlayerController();
	UWorld*                  World = GetWorld();
	URetrieveMapSubsystem* MapSub = World ? World->GetSubsystem<URetrieveMapSubsystem>() : nullptr;

	// ── 레벨 종횡비를 반영한 맵 표시 크기 계산 ──────────────────────────────
	// BaseW/BaseH: ZoomLevel=1 기준, MapSide 안에 레벨 비율대로 최대 채움
	// ScaledW/H  : 실제 그릴 픽셀 크기 (줌 적용)
	float BaseW = MapViewSize.X, BaseH = MapViewSize.Y;
	ComputeBaseMapSize(MapSub, MapViewSize, BaseW, BaseH);

	const float     ScaledW = BaseW * ZoomLevel;
	const float     ScaledH = BaseH * ZoomLevel;

	// HitTestBonfireIcon이 동일한 좌표계를 쓸 수 있도록 캐시
	CachedPaintCenter  = Center;
	CachedPaintScaledW = ScaledW;
	CachedPaintScaledH = ScaledH;

	const FVector2D MapTopLeft(
		Center.X - ScaledW * ViewCenterUV.X,
		Center.Y - ScaledH * ViewCenterUV.Y
	);
	const FVector2D MapDrawSize(ScaledW, ScaledH);

	// 클리핑: 맵 뷰포트 영역과 위젯 전체 경계의 교집합으로 클리핑.
	// 뷰포트 위젯 바인딩 오류 등으로 맵 영역이 위젯 밖을 넘어도 텍스처가 벗어나지 않음.
	const FVector2D AbsWidgetOrigin  = FVector2D(AllottedGeometry.LocalToAbsolute(FVector2f::ZeroVector));
	const FVector2D AbsWidgetEnd     = FVector2D(AllottedGeometry.LocalToAbsolute(FVector2f(WidgetSize)));
	const FSlateRect WidgetAbsRect(AbsWidgetOrigin.X, AbsWidgetOrigin.Y, AbsWidgetEnd.X, AbsWidgetEnd.Y);

	const FVector2D AbsMapTopLeft     = FVector2D(AllottedGeometry.LocalToAbsolute(FVector2f(MapViewTopLeft)));
	const FVector2D AbsMapBottomRight = FVector2D(AllottedGeometry.LocalToAbsolute(FVector2f(MapViewTopLeft + MapViewSize)));
	FSlateRect MapViewAbsRect(AbsMapTopLeft.X, AbsMapTopLeft.Y, AbsMapBottomRight.X, AbsMapBottomRight.Y);
	MapViewAbsRect = MapViewAbsRect.IntersectionWith(WidgetAbsRect);
	const FSlateClippingZone ClipZone(MapViewAbsRect);
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

	const FSlateRect MapViewRect(MapViewTopLeft.X, MapViewTopLeft.Y,
	                             MapViewTopLeft.X + MapViewSize.X, MapViewTopLeft.Y + MapViewSize.Y);

	// ── DataAsset 기반 정적 아이콘 (WP 로드 여부 완전 무관) ────────────────────
	// 화톳불 포함 모든 아이콘 항상 표시. Activated 여부는 히트 판정(HitTestBonfireIcon)에서 체크.
	if (MapSub && MapSub->HasValidBounds() && WorldMapIconData)
	{
		for (const FRetrieveMapIconEntry& Entry : WorldMapIconData->Icons)
		{
			if (HiddenIconTypesOnWorldMap.Contains(Entry.IconType)) { continue; }

			const FVector2D IconUV     = MapSub->WorldToUV(Entry.WorldLocation);
			const FVector2D IconScreen = UVToScreen(IconUV, Center, ScaledW, ScaledH);

			if (!MapViewRect.ContainsPoint(FVector2D(IconScreen))) { continue; }

			FRetrieveMapIconSnapshot Snap;
			Snap.WorldLocation        = Entry.WorldLocation;
			Snap.IconType             = Entry.IconType;
			Snap.MapLabel             = Entry.MapLabel;
			Snap.bShowLabelOnWorldMap = Entry.bShowLabel;

			DrawWorldIcon(OutDrawElements, CurrentLayer, AllottedGeometry, Snap, IconScreen);

			if (!Entry.MapLabel.IsEmpty() && Entry.bShowLabel)
			{
				float IconHalfSize = 8.0f;
				if (IconRegistry)
				{
					IconHalfSize = IconRegistry->FindRow(Entry.IconType).IconSize * 0.5f;
				}
				const FSlateFontInfo IconFont =
					FCoreStyle::GetDefaultFontStyle("Regular", IconLabelFontSize);
				DrawLabel(OutDrawElements, CurrentLayer, AllottedGeometry,
				          Entry.MapLabel.ToString(),
				          FVector2D(IconScreen.X, IconScreen.Y + IconHalfSize + 4.0f),
				          IconFont, LabelColor);
			}
		}
	}

	// ── 플레이어 마커 + 현재 위치 레이블 ─────────────────────────────────────
	if (PC && PC->GetPawn() && MapSub && MapSub->HasValidBounds())
	{
		const FVector2D PlayerUV     = MapSub->WorldToUV(PC->GetPawn()->GetActorLocation());
		const FVector2D PlayerScreen = UVToScreen(PlayerUV, Center, ScaledW, ScaledH);

		if (MapViewRect.ContainsPoint(FVector2D(PlayerScreen)))
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

			const FLinearColor EffectiveMarkerColor =
				PlayerMarkerColor.A > KINDA_SMALL_NUMBER ? PlayerMarkerColor : FLinearColor::White;
			const FLinearColor EffectiveLabelColor =
				LabelColor.A > KINDA_SMALL_NUMBER ? LabelColor : FLinearColor::White;

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
				EffectiveMarkerColor
			);

			const FText LocationText = CurrentLocationText.IsEmpty()
				? NSLOCTEXT("RetrieveWorldMap", "CurrentLocation", "현재 위치")
				: CurrentLocationText;
			if (!LocationText.IsEmpty())
			{
				const FSlateFontInfo LabelFont =
					FCoreStyle::GetDefaultFontStyle("Bold", PlayerLabelFontSize);
				DrawLabel(OutDrawElements, CurrentLayer, AllottedGeometry,
				          LocationText.ToString(),
				          FVector2D(PlayerScreen.X, PlayerScreen.Y - MarkerSz.Y * 0.5f - 6.0f),
				          LabelFont, EffectiveLabelColor);
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

FReply URetrieveWorldMapWidget::NativeOnMouseButtonDoubleClick(
	const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FReply::Unhandled();
	}

	const FVector2D LocalPos = InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

	FVector2D MapViewTopLeft, MapViewSize;
	GetMapViewRect(InGeometry, MapViewTopLeft, MapViewSize);
	if (!IsInsideMapView(LocalPos, MapViewTopLeft, MapViewSize))
	{
		return FReply::Unhandled();
	}

	if (TryBroadcastBonfireDoubleClick(InGeometry, LocalPos))
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply URetrieveWorldMapWidget::NativeOnPreviewMouseButtonDown(
	const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
	}

	const FVector2D LocalMousePos =
		InGeometry.AbsoluteToLocal(InMouseEvent.GetScreenSpacePosition());

	FVector2D MapViewTopLeft, MapViewSize;
	GetMapViewRect(InGeometry, MapViewTopLeft, MapViewSize);
	if (!IsInsideMapView(LocalMousePos, MapViewTopLeft, MapViewSize))
	{
		return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
	}

	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

FReply URetrieveWorldMapWidget::NativeOnMouseButtonDown(
	const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 더블클릭이 화톳불 아이콘을 소비했으면 패닝 시작 억제
	if (bDoubleClickConsumed)
	{
		bDoubleClickConsumed = false;
		return FReply::Handled();
	}

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

	if (MapViewport)
	{
		const FGeometry& ViewGeometry = MapViewport->GetCachedGeometry();
		const FVector2D ViewSize = ViewGeometry.GetLocalSize();
		if (ViewSize.X > 1.0f && ViewSize.Y > 1.0f)
		{
			const FVector2D AbsTopLeft = FVector2D(ViewGeometry.LocalToAbsolute(FVector2f::ZeroVector));
			const FVector2D AbsBottomRight = FVector2D(ViewGeometry.LocalToAbsolute(FVector2f(ViewSize)));
			FVector2D CandidateTopLeft = AllottedGeometry.AbsoluteToLocal(AbsTopLeft);
			const FVector2D CandidateBottomRight = AllottedGeometry.AbsoluteToLocal(AbsBottomRight);

			const FSlateRect WidgetRect(0.0f, 0.0f, WidgetSize.X, WidgetSize.Y);
			FSlateRect CandidateRect(
				CandidateTopLeft.X,
				CandidateTopLeft.Y,
				CandidateBottomRight.X,
				CandidateBottomRight.Y
			);
			CandidateRect = CandidateRect.IntersectionWith(WidgetRect);

			const float CandidateW = CandidateRect.Right - CandidateRect.Left;
			const float CandidateH = CandidateRect.Bottom - CandidateRect.Top;
			if (CandidateW > 1.0f && CandidateH > 1.0f)
			{
				OutTopLeft = FVector2D(CandidateRect.Left, CandidateRect.Top);
				OutSize = FVector2D(CandidateW, CandidateH);
				return;
			}
		}
	}

	// 폴백: Border_Window의 퍼센트 앵커 + MapViewportPadding으로 해상도 독립적 계산.
	// MapViewport->GetCachedGeometry()가 (0,0)일 때(빈 CanvasPanel 최적화)도 항상 정확.
	const float BW_Left   = WidgetSize.X * MapWindowAnchorMin.X;
	const float BW_Top    = WidgetSize.Y * MapWindowAnchorMin.Y;
	const float BW_Right  = WidgetSize.X * MapWindowAnchorMax.X;
	const float BW_Bottom = WidgetSize.Y * MapWindowAnchorMax.Y;

	OutTopLeft = FVector2D(
		BW_Left + MapViewportPadding.Left,
		BW_Top  + MapViewportPadding.Top);
	OutSize = FVector2D(
		FMath::Max(1.0f, (BW_Right  - BW_Left) - MapViewportPadding.Left - MapViewportPadding.Right),
		FMath::Max(1.0f, (BW_Bottom - BW_Top)  - MapViewportPadding.Top  - MapViewportPadding.Bottom));
}

APlayerController* URetrieveWorldMapWidget::GetWorldMapPlayerController() const
{
	if (APlayerController* OwningPC = GetOwningPlayer())
	{
		return OwningPC;
	}

	return UGameplayStatics::GetPlayerController(this, 0);
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
	const FRetrieveMapIconSnapshot& Snap,
	const FVector2D& ScreenPos
) const
{
	UTexture2D*  Texture = nullptr;
	FLinearColor Color   = FLinearColor::White;
	float        Size    = 16.0f;

	if (Snap.bOverrideIcon)
	{
		Texture = Snap.OverrideTexture;
		Color   = Snap.OverrideColor;
		Size    = Snap.OverrideSize;
	}
	else if (IconRegistry)
	{
		const FRetrieveMapIconRow& Row = IconRegistry->FindRow(Snap.IconType);
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

const URetrieveMapIconComponent* URetrieveWorldMapWidget::HitTestBonfireIcon(
	const FGeometry& InGeometry, const FVector2D& HitPos) const
{
	UWorld* World = GetWorld();
	URetrieveMapSubsystem* MapSub = World ? World->GetSubsystem<URetrieveMapSubsystem>() : nullptr;
	if (!MapSub || !MapSub->HasValidBounds()) { return nullptr; }

	FVector2D MapViewTopLeft, MapViewSize;
	// NativePaint 캐시 파라미터 사용 → 드로잉과 동일한 좌표계로 히트 판정
	const FVector2D Center  = CachedPaintScaledW > 1.0f ? CachedPaintCenter
	                        : (MapViewTopLeft + MapViewSize * 0.5f);
	const float ScaledW = CachedPaintScaledW > 1.0f ? CachedPaintScaledW
	                    : ([&]{ float BaseW = MapViewSize.X, BaseH = MapViewSize.Y;
	                            ComputeBaseMapSize(MapSub, MapViewSize, BaseW, BaseH);
	                            return BaseW * ZoomLevel; }());
	const float ScaledH = CachedPaintScaledH > 1.0f ? CachedPaintScaledH
	                    : ([&]{ float BaseW = MapViewSize.X, BaseH = MapViewSize.Y;
	                            ComputeBaseMapSize(MapSub, MapViewSize, BaseW, BaseH);
	                            return BaseH * ZoomLevel; }());

	auto GetHitRadiusSq = [this](const URetrieveMapIconComponent* Icon)
	{
		float Radius = FMath::Max(BonfireIconHitRadius, 48.0f);
		if (Icon)
		{
			if (Icon->bOverrideIcon)
			{
				Radius = FMath::Max(Radius, Icon->OverrideSize * 0.75f);
			}
			else if (IconRegistry)
			{
				Radius = FMath::Max(
					Radius,
					IconRegistry->FindRow(Icon->IconType).IconSize * 0.75f);
			}
		}
		return FMath::Square(Radius);
	};

	for (const URetrieveMapIconComponent* Icon : MapSub->GetIcons())
	{
		if (!IsValid(Icon) || !IsValid(Icon->GetOwner())) { continue; }
		const ARetrieveBonfireActor* Bonfire = Cast<ARetrieveBonfireActor>(Icon->GetOwner());
		if (!Bonfire || !Bonfire->IsActivated()) { continue; }

		const FVector2D IconUV     = MapSub->WorldToUV(Icon->GetOwner()->GetActorLocation());
		const FVector2D IconScreen = UVToScreen(IconUV, Center, ScaledW, ScaledH);
		const FVector2D Delta      = HitPos - IconScreen;

		if (Delta.SizeSquared() <= GetHitRadiusSq(Icon))
		{
			return Icon;
		}
	}

	for (TActorIterator<ARetrieveBonfireActor> It(World); It; ++It)
	{
		const ARetrieveBonfireActor* Bonfire = *It;
		if (!IsValid(Bonfire) || !Bonfire->IsActivated() ||
			!IsValid(Bonfire->MapIconComponent))
		{
			continue;
		}

		const FVector2D IconUV = MapSub->WorldToUV(Bonfire->GetActorLocation());
		const FVector2D IconScreen = UVToScreen(IconUV, Center, ScaledW, ScaledH);
		const float DistanceSq = FVector2D::DistSquared(HitPos, IconScreen);
		if (DistanceSq <= GetHitRadiusSq(Bonfire->MapIconComponent))
		{
			return Bonfire->MapIconComponent;
		}

		UE_LOG(LogTemp, Log,
			TEXT("[WorldMap] Bonfire hit miss - BonfireId=%s Icon=(%.1f, %.1f) Hit=(%.1f, %.1f) Distance=%.1f"),
			*Bonfire->BonfireId.ToString(),
			IconScreen.X,
			IconScreen.Y,
			HitPos.X,
			HitPos.Y,
			FMath::Sqrt(DistanceSq));
	}

	return nullptr;
}

bool URetrieveWorldMapWidget::TryBroadcastBonfireDoubleClick(
	const FGeometry& InGeometry, const FVector2D& HitPos)
{
	// ── 1차: 로드된 액터에서 탐색 ────────────────────────────────────────────
	const URetrieveMapIconComponent* HitIcon = HitTestBonfireIcon(InGeometry, HitPos);
	if (HitIcon)
	{
		const ARetrieveBonfireActor* Bonfire = Cast<ARetrieveBonfireActor>(HitIcon->GetOwner());
		if (Bonfire && Bonfire->IsActivated())
		{
			bDoubleClickConsumed = true;
			LastClickedBonfireId = NAME_None;
			LastBonfireIconClickTime = -1.0;
			UE_LOG(LogTemp, Log, TEXT("[WorldMap] 모닥불 더블클릭(로드) — BonfireId=%s"), *Bonfire->BonfireId.ToString());
			if (TryOpenFastTravelDialog(*Bonfire)) { return true; }
			OnBonfireIconDoubleClicked.Broadcast(Bonfire->BonfireId, Bonfire->DisplayName);
			return true;
		}
	}

	// ── 2차: 언로드된 화톳불 — DataAsset 위치 + SaveSubsystem 활성화 여부 ────
	if (!WorldMapIconData) { return false; }

	const UGameInstance* GI = GetGameInstance();
	const URetrieveSaveSubsystem* SaveSub = GI ? GI->GetSubsystem<URetrieveSaveSubsystem>() : nullptr;
	const URetrieveSaveGame* SaveGame = SaveSub ? SaveSub->GetCurrentSaveGame() : nullptr;
	if (!SaveGame) { return false; }

	UWorld* World = GetWorld();
	URetrieveMapSubsystem* MapSub = World ? World->GetSubsystem<URetrieveMapSubsystem>() : nullptr;
	if (!MapSub || !MapSub->HasValidBounds()) { return false; }

	// 드로잉과 동일한 좌표계 사용
	const FVector2D Center  = CachedPaintCenter;
	const float     ScaledW = CachedPaintScaledW;
	const float     ScaledH = CachedPaintScaledH;

	const float HitRadiusSq = FMath::Square(FMath::Max(BonfireIconHitRadius, 48.0f));

	for (const FRetrieveMapIconEntry& Entry : WorldMapIconData->Icons)
	{
		if (Entry.IconType != ERetrieveMapIconType::Bonfire) { continue; }

		const FVector2D IconUV     = MapSub->WorldToUV(Entry.WorldLocation);
		const FVector2D IconScreen = UVToScreen(IconUV, Center, ScaledW, ScaledH);

		if (FVector2D::DistSquared(HitPos, IconScreen) > HitRadiusSq) { continue; }

		// 이 위치가 활성화된 화톳불인지 SaveSubsystem에서 확인 (위치 거리 비교)
		for (const TPair<FName, FTransform>& Pair : SaveGame->ActivatedBonfireTransforms)
		{
			if (FVector::DistSquared(Entry.WorldLocation, Pair.Value.GetLocation()) > FMath::Square(200.f))
			{
				continue;
			}

			// 활성화된 화톳불 확인 → 빠른 이동 창 오픈
			bDoubleClickConsumed = true;
			LastClickedBonfireId = NAME_None;
			LastBonfireIconClickTime = -1.0;

			// 이름은 Entry.MapLabel 사용 (액터 언로드 상태이므로)
			const FText DisplayName = Entry.MapLabel;
			UE_LOG(LogTemp, Log, TEXT("[WorldMap] 모닥불 더블클릭(언로드) — BonfireId=%s"), *Pair.Key.ToString());

			// 빠른 이동 확인 창 직접 오픈
			APlayerController* PC = GetWorldMapPlayerController();
			UClass* DialogClass = FastTravelDialogClass.LoadSynchronous();
			if (!PC || !DialogClass) { break; }

			CloseActiveFastTravelDialog();
			UUserWidget* Dialog = CreateWidget<UUserWidget>(PC, DialogClass);
			if (!Dialog) { break; }

			struct FInitParams { FName BonfireId; FText BonfireDisplayName; };
			FInitParams Params{ Pair.Key, DisplayName };
			if (UFunction* Fn = Dialog->FindFunction(TEXT("InitDialog")))
			{
				Dialog->ProcessEvent(Fn, &Params);
			}
			ActiveFastTravelDialog   = Dialog;
			ActiveFastTravelBonfireId = Pair.Key;
			Dialog->AddToViewport(100);

			if (UButton* Btn = Cast<UButton>(Dialog->GetWidgetFromName(TEXT("Button_Confirm"))))
			{
				Btn->OnClicked.Clear();
				Btn->OnClicked.AddDynamic(this, &ThisClass::HandleFastTravelConfirmClicked);
			}
			if (UButton* Btn = Cast<UButton>(Dialog->GetWidgetFromName(TEXT("Button_Cancel"))))
			{
				Btn->OnClicked.Clear();
				Btn->OnClicked.AddDynamic(this, &ThisClass::HandleFastTravelCancelClicked);
			}
			return true;
		}
	}

	return false;
}

bool URetrieveWorldMapWidget::HandleBonfireIconClick(
	const FGeometry& InGeometry, const FVector2D& HitPos)
{
	FVector2D MapViewTopLeft, MapViewSize;
	GetMapViewRect(InGeometry, MapViewTopLeft, MapViewSize);
	if (!IsInsideMapView(HitPos, MapViewTopLeft, MapViewSize))
	{
		UE_LOG(LogTemp, Log,
			TEXT("[WorldMap] Mouse press outside map viewport - Hit=(%.1f, %.1f) Rect=(%.1f, %.1f, %.1f, %.1f)"),
			HitPos.X,
			HitPos.Y,
			MapViewTopLeft.X,
			MapViewTopLeft.Y,
			MapViewSize.X,
			MapViewSize.Y);
		return false;
	}

	const URetrieveMapIconComponent* HitIcon = HitTestBonfireIcon(InGeometry, HitPos);
	const ARetrieveBonfireActor* Bonfire =
		HitIcon ? Cast<ARetrieveBonfireActor>(HitIcon->GetOwner()) : nullptr;

	if (!Bonfire || !Bonfire->IsActivated())
	{
		UE_LOG(LogTemp, Log,
			TEXT("[WorldMap] No activated bonfire icon at mouse press - Hit=(%.1f, %.1f)"),
			HitPos.X,
			HitPos.Y);
		LastClickedBonfireId = NAME_None;
		LastBonfireIconClickTime = -1.0;
		return false;
	}

	const double CurrentClickTime = FPlatformTime::Seconds();
	const bool bIsRepeatedBonfireClick =
		LastClickedBonfireId == Bonfire->BonfireId
		&& CurrentClickTime - LastBonfireIconClickTime <= BonfireDoubleClickInterval
		&& FVector2D::DistSquared(HitPos, LastBonfireIconClickPos)
			<= FMath::Square(FMath::Max(BonfireIconHitRadius, 48.0f));

	LastClickedBonfireId = Bonfire->BonfireId;
	LastBonfireIconClickTime = CurrentClickTime;
	LastBonfireIconClickPos = HitPos;

	if (bIsRepeatedBonfireClick)
	{
		return TryBroadcastBonfireDoubleClick(InGeometry, HitPos);
	}

	UE_LOG(LogTemp, Log, TEXT("[WorldMap] 모닥불 아이콘 첫 클릭 — BonfireId=%s"),
		*Bonfire->BonfireId.ToString());
	return false;
}

bool URetrieveWorldMapWidget::TryOpenFastTravelDialog(const ARetrieveBonfireActor& Bonfire)
{
	APlayerController* PC = GetWorldMapPlayerController();
	UClass* DialogClass = FastTravelDialogClass.LoadSynchronous();
	if (!DialogClass)
	{
		DialogClass = LoadClass<UUserWidget>(
			nullptr,
			TEXT("/Game/Retrieve/UI/HUD/WBP_FastTravelDialog.WBP_FastTravelDialog_C"));
	}
	if (!PC || !DialogClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[WorldMap] 빠른 이동 확인 창 생성 실패 — PC=%s DialogClass=%s"),
			PC ? TEXT("Valid") : TEXT("None"),
			DialogClass ? *DialogClass->GetName() : TEXT("None"));
		return false;
	}

	UUserWidget* Dialog = CreateWidget<UUserWidget>(PC, DialogClass);
	if (!Dialog)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WorldMap] 빠른 이동 확인 창 CreateWidget 실패"));
		return false;
	}

	UFunction* InitDialogFunction = Dialog->FindFunction(TEXT("InitDialog"));
	if (!InitDialogFunction)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[WorldMap] 빠른 이동 확인 창에 InitDialog 이벤트가 없음 — Class=%s"),
			*DialogClass->GetName());
		return false;
	}

	struct FInitDialogParams
	{
		FName BonfireId;
		FText BonfireDisplayName;
	};

	FInitDialogParams Params;
	Params.BonfireId = Bonfire.BonfireId;
	Params.BonfireDisplayName = Bonfire.DisplayName;

	CloseActiveFastTravelDialog();
	ActiveFastTravelDialog = Dialog;
	ActiveFastTravelBonfireId = Bonfire.BonfireId;

	Dialog->AddToViewport(100);
	Dialog->ProcessEvent(InitDialogFunction, &Params);

	if (UButton* ConfirmButton = Cast<UButton>(Dialog->GetWidgetFromName(TEXT("Button_Confirm"))))
	{
		ConfirmButton->OnClicked.Clear();
		ConfirmButton->OnClicked.AddDynamic(this, &ThisClass::HandleFastTravelConfirmClicked);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[WorldMap] Fast travel dialog has no Button_Confirm"));
	}

	if (UButton* CancelButton = Cast<UButton>(Dialog->GetWidgetFromName(TEXT("Button_Cancel"))))
	{
		CancelButton->OnClicked.Clear();
		CancelButton->OnClicked.AddDynamic(this, &ThisClass::HandleFastTravelCancelClicked);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[WorldMap] Fast travel dialog has no Button_Cancel"));
	}

	UE_LOG(LogTemp, Log, TEXT("[WorldMap] 빠른 이동 확인 창 오픈 — BonfireId=%s"),
		*Bonfire.BonfireId.ToString());
	return true;
}

void URetrieveWorldMapWidget::HandleFastTravelConfirmClicked()
{
	const FName TargetBonfireId = ActiveFastTravelBonfireId;
	APlayerController* PC = GetWorldMapPlayerController();
	CloseActiveFastTravelDialog();

	UGameInstance* GameInstance = GetGameInstance();
	URetrieveSaveSubsystem* SaveSubsystem =
		GameInstance ? GameInstance->GetSubsystem<URetrieveSaveSubsystem>() : nullptr;
	FTransform ArrivalTransform;
	if (!PC || TargetBonfireId.IsNone() || !SaveSubsystem ||
		!SaveSubsystem->GetBonfireTransform(TargetBonfireId, ArrivalTransform))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[WorldMap] Fast travel confirm failed - PC=%s SaveSubsystem=%s BonfireId=%s"),
			PC ? TEXT("Valid") : TEXT("None"),
			SaveSubsystem ? TEXT("Valid") : TEXT("None"),
			*TargetBonfireId.ToString());
		return;
	}

	if (ARetrievePlayerController* RetrievePC = Cast<ARetrievePlayerController>(PC))
	{
		RetrievePC->CloseActivePanel();
	}

	ShowFastTravelLoadingOverlay(PC);
	SaveSubsystem->OnFastTravelCompleted.RemoveDynamic(
		this, &ThisClass::HandleFastTravelCompleted);
	SaveSubsystem->OnFastTravelCompleted.AddDynamic(
		this, &ThisClass::HandleFastTravelCompleted);
	SaveSubsystem->FastTravelToBonfire(TargetBonfireId, PC);
	UE_LOG(LogTemp, Log, TEXT("[WorldMap] Fast travel confirmed - BonfireId=%s"),
		*TargetBonfireId.ToString());
}

void URetrieveWorldMapWidget::HandleFastTravelCancelClicked()
{
	UE_LOG(LogTemp, Log, TEXT("[WorldMap] Fast travel canceled - BonfireId=%s"),
		*ActiveFastTravelBonfireId.ToString());
	CloseActiveFastTravelDialog();
}

void URetrieveWorldMapWidget::HandleFastTravelCompleted()
{
	if (IsValid(ActiveFastTravelLoadingOverlay))
	{
		ActiveFastTravelLoadingOverlay->RemoveFromParent();
	}
	ActiveFastTravelLoadingOverlay = nullptr;

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (URetrieveSaveSubsystem* SaveSubsystem =
			GameInstance->GetSubsystem<URetrieveSaveSubsystem>())
		{
			SaveSubsystem->OnFastTravelCompleted.RemoveDynamic(
				this, &ThisClass::HandleFastTravelCompleted);
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[WorldMap] Fast travel loading overlay closed"));
}

void URetrieveWorldMapWidget::CloseActiveFastTravelDialog()
{
	if (IsValid(ActiveFastTravelDialog))
	{
		ActiveFastTravelDialog->RemoveFromParent();
	}
	ActiveFastTravelDialog = nullptr;
	ActiveFastTravelBonfireId = NAME_None;
}

void URetrieveWorldMapWidget::ShowFastTravelLoadingOverlay(APlayerController* PC)
{
	if (!PC || (IsValid(ActiveFastTravelLoadingOverlay) &&
		ActiveFastTravelLoadingOverlay->IsInViewport()))
	{
		return;
	}

	UClass* LoadingClass = FastTravelLoadingClass.LoadSynchronous();
	if (!LoadingClass)
	{
		LoadingClass = LoadClass<UUserWidget>(
			nullptr,
			TEXT("/Game/Retrieve/UI/HUD/WBP_LoadingCutscene.WBP_LoadingCutscene_C"));
	}

	if (!LoadingClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WorldMap] Fast travel loading overlay class not found"));
		return;
	}

	ActiveFastTravelLoadingOverlay = CreateWidget<UUserWidget>(PC, LoadingClass);
	if (ActiveFastTravelLoadingOverlay)
	{
		ActiveFastTravelLoadingOverlay->AddToViewport(200);
	}
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
