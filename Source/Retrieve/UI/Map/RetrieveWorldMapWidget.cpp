#include "UI/Map/RetrieveWorldMapWidget.h"
#include "Subsystems/RetrieveMapSubsystem.h"
#include "UI/RetrieveUISettingsLibrary.h"
#include "InputCoreTypes.h"
#include "Components/World/RetrieveMapIconComponent.h"
#include "Data/RetrieveMapIconRegistry.h"
#include "World/RetrieveBonfireActor.h"
#include "Save/RetrieveSaveSubsystem.h"
#include "Player/RetrievePlayerController.h"
#include "GameplayTags/RetrieveGameplayTags.h"

#include "Camera/PlayerCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"
#include "Engine/Texture2D.h"
#include "EngineUtils.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "HAL/PlatformTime.h"
#include "Data/RetrieveMapConfigDataAsset.h"
#include "UObject/ConstructorHelpers.h"

FGameplayTag URetrieveWorldMapWidget::GetPanelOpenSoundContext() const
{
	return RetrieveGameplayTags::UI_Sound_WorldMap_Open;
}

FGameplayTag URetrieveWorldMapWidget::GetPanelCloseSoundContext() const
{
	return RetrieveGameplayTags::UI_Sound_WorldMap_Close;
}

// ---------- 초기화 ----------

URetrieveWorldMapWidget::URetrieveWorldMapWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UTexture2D> FogTextureFinder(
		TEXT("/Game/Retrieve/UI/Map/Textures/T_WorldMapFog.T_WorldMapFog"));
	if (FogTextureFinder.Succeeded())
	{
		FogTexture = FogTextureFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FogMaterialFinder(
		TEXT("/Game/Retrieve/UI/Map/Materials/M_WorldMapFogAnimated.M_WorldMapFogAnimated"));
	if (FogMaterialFinder.Succeeded())
	{
		FogDisplayMaterial = FogMaterialFinder.Object;
	}
}

void URetrieveWorldMapWidget::NativeDestruct()
{
	// 월드맵이 닫힐 때 확인 다이얼로그는 정리한다.
	CloseActiveFastTravelDialog();

	// 주의: 로딩 오버레이는 여기서 제거하지 않는다.
	// 빠른 이동을 확정하면 CloseActivePanel()로 이 월드맵 위젯이 곧 GC되는데,
	// 여기서 오버레이를 제거하면 빠른 이동(스트리밍/텔레포트)이 끝나기도 전에
	// 로딩화면이 사라져 버린다. 오버레이는 자신의 OnFastTravelCompleted 바인딩(WBP 내부)으로
	// 빠른 이동 완료 시점에 스스로 제거되므로 월드맵 수명과 분리한다.
	// (참조만 끊어 둔다 — 뷰포트에 add되어 있으므로 GC되지 않는다.)
	ActiveFastTravelLoadingOverlay = nullptr;

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (URetrieveSaveSubsystem* SaveSubsystem = GameInstance->GetSubsystem<URetrieveSaveSubsystem>())
		{
			SaveSubsystem->OnFastTravelCompleted.RemoveDynamic(this, &ThisClass::HandleFastTravelCompleted);
		}
	}

	Super::NativeDestruct();
}

void URetrieveWorldMapWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetVisibility(ESlateVisibility::Visible);
	SetClipping(EWidgetClipping::ClipToBoundsAlways);
	// Reserve side gutters for the legend and controls. Keep the source map aspect ratio.
	// (MapOffsets는 아래 MapViewportPadding에서도 쓰므로 if 블록 밖에 선언한다.)
	const FMargin MapOffsets(240.0f, 84.0f, 240.0f, 54.0f);

	// 레전드/컨트롤 패널이 없는 WBP에서만 슬롯 오프셋을 코드로 적용한다.
	// (여는 중괄호가 빠져 함수가 조기 종료되던 것을 복구 — 아래 단독 '}'가 이 블록의 닫는 괄호)
	if (!GetWidgetFromName(TEXT("Panel_MapLegend")) && !GetWidgetFromName(TEXT("Panel_MapControls")))
	{
	if (MapViewport)
	{
		if (UCanvasPanelSlot* MapSlot = Cast<UCanvasPanelSlot>(MapViewport->Slot))
		{
			MapSlot->SetOffsets(MapOffsets);
		}
	}
	if (UWidget* MapShadow = GetWidgetFromName(TEXT("IMG_MapScrollShadow")))
	{
		if (UCanvasPanelSlot* ShadowSlot = Cast<UCanvasPanelSlot>(MapShadow->Slot))
		{
			ShadowSlot->SetOffsets(FMargin(224.0f, 48.0f, 224.0f, 18.0f));
		}
	}
	if (UWidget* MapPaper = GetWidgetFromName(TEXT("IMG_MapScrollPaper")))
	{
		if (UCanvasPanelSlot* PaperSlot = Cast<UCanvasPanelSlot>(MapPaper->Slot))
		{
			PaperSlot->SetOffsets(FMargin(232.0f, 56.0f, 232.0f, 26.0f));
		}
	}

	}
	// Fit the complete source texture inside the viewport instead of stretching it.
	bStretchMapToViewport = false;
	MapViewportPadding = MapOffsets;
	ZoomLevel    = MinZoom;
	ViewCenterUV = FVector2D(0.5f, 0.5f);
	bPendingCenterOnPlayer = true;
	bWasLeftMouseButtonDown = false;

	const FString LocationString = CurrentLocationText.ToString();
	if (LocationString.IsEmpty() || LocationString.Contains(TEXT("?")))
	{
		CurrentLocationText = NSLOCTEXT("RetrieveWorldMap", "CurrentLocation", "현재 위치");
	}

	// 월드맵을 열 때마다 MapConfig 기준으로 텍스처/아이콘 설정을 동기화하고,
	// 현재 로드된 모든 MapIconComponent를 스캔해서 스냅샷 갱신.
	if (UWorld* World = GetWorld())
	{
		if (URetrieveMapSubsystem* MapSub = World->GetSubsystem<URetrieveMapSubsystem>())
		{
			MapSub->InitializeMapConfigRuntime(MapConfig);
			if (MapSub->MapConfig)
			{
				if (!BakedMapTexture)
				{
					BakedMapTexture = MapSub->MapConfig->BakedMapTexture;
				}

				if (!WorldMapIconData)
				{
					WorldMapIconData = MapSub->MapConfig->WorldMapIconData;
				}

				if (!IconRegistry)
				{
					IconRegistry = MapSub->MapConfig->IconRegistry;
				}
			}

			MapSub->RefreshWorldMapSnapshots();
		}
	}

	// 에디터에서 활성으로 배치된 모닥불을 세이브 서브시스템에 인메모리 시드.
	// → WP 스트리밍/액터 로드 여부와 무관하게 활성 표시 + 빠른이동 가능.
	SeedDefaultActivatedBonfires();
}

void URetrieveWorldMapWidget::SeedDefaultActivatedBonfires()
{
	if (!WorldMapIconData) { return; }

	UGameInstance* GI = GetGameInstance();
	URetrieveSaveSubsystem* SaveSub = GI ? GI->GetSubsystem<URetrieveSaveSubsystem>() : nullptr;
	if (!SaveSub) { return; }

	for (const FRetrieveMapIconEntry& Entry : WorldMapIconData->Icons)
	{
		if (Entry.IconType != ERetrieveMapIconType::Bonfire) { continue; }
		if (!Entry.bStartActivated || Entry.BonfireId.IsNone()) { continue; }

		// ArrivalTransform이 (0,0,0)로 구워진 경우(에디터 스캔 시 ArrivalPoint 미확정 등)
		// 모닥불 본체 위치(WorldLocation)를 도착 기준으로 폴백한다. Z는 텔레포트 시 지면 스냅으로 보정.
		FTransform Arrival = Entry.ArrivalTransform;
		if (Arrival.GetLocation().IsNearlyZero())
		{
			Arrival = FTransform(Entry.WorldLocation);
		}

		// 이미 활성 기록이 있으면 RegisterDefaultBonfire 내부에서 덮어쓰지 않는다.
		SaveSub->RegisterDefaultBonfire(Entry.BonfireId, Arrival);
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
	// ── 1) 기준(fit) 크기 계산 ────────────────────────────────────────────────
	if (bStretchMapToViewport)
	{
		OutBaseW = FMath::Max(MapViewSize.X, 1.0f);
		OutBaseH = FMath::Max(MapViewSize.Y, 1.0f);
	}
	else
	{
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
		}
		else
		{
			const float MapSide = FMath::Min(MapViewSize.X, MapViewSize.Y);

			// 기본값: 정사각
			OutBaseW = OutBaseH = MapSide;

			if (MapSub && MapSub->HasValidBounds())
			{
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
		}
	}

	// ── 2) 사용자 조정 배율 적용 (텍스처·아이콘 공통, 단일 출구) ────────────────
	// 아이콘은 ScaledW/ScaledH(= Base * Zoom)로 그려지므로 여기서 한 번만 곱하면
	// 텍스처와 아이콘이 동일 배율로 스케일되어 정합이 항상 유지된다.
	OutBaseW *= FMath::Max(MapTextureScale.X, 0.01f);
	OutBaseH *= FMath::Max(MapTextureScale.Y, 0.01f);
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

	const FVector2D Center  = GetMapDrawCenter(MapViewTopLeft, MapViewSize);

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
	const FSlateRect IntersectedRect = MapViewAbsRect.IntersectionWith(WidgetAbsRect);

	// 가드: 맵 뷰 rect가 위젯과 거의 겹치지 않으면(=좌표 계산이 어긋나 맵이 패널 밖으로 나간 경우)
	// 텍스처가 통째로 클립되어 빈 화면이 된다. 이때는 위젯 전체로 클립을 폴백해 최소한 보이게 하고 경고 로그.
	const float IntersectW = IntersectedRect.Right - IntersectedRect.Left;
	const float IntersectH = IntersectedRect.Bottom - IntersectedRect.Top;
	if (IntersectW <= 1.0f || IntersectH <= 1.0f)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[WorldMap] Map view rect outside widget — clip fallback to full widget. "
			     "MapView=(%.1f, %.1f, %.1f, %.1f) Widget=(%.1f, %.1f, %.1f, %.1f)"),
			MapViewAbsRect.Left, MapViewAbsRect.Top, MapViewAbsRect.Right, MapViewAbsRect.Bottom,
			WidgetAbsRect.Left, WidgetAbsRect.Top, WidgetAbsRect.Right, WidgetAbsRect.Bottom);
		MapViewAbsRect = WidgetAbsRect;
	}
	else
	{
		MapViewAbsRect = IntersectedRect;
	}
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

			// 표시 머티리얼이 있으면 알파 반전(빈 공간 투명) 머티리얼로 그린다.
			if (MapDisplayMaterial)
			{
				if (!MapDisplayMID || MapDisplayMID->Parent != MapDisplayMaterial)
				{
					MapDisplayMID = UMaterialInstanceDynamic::Create(
						MapDisplayMaterial, const_cast<URetrieveWorldMapWidget*>(this));
				}
				if (MapDisplayMID)
				{
					MapDisplayMID->SetTextureParameterValue(TEXT("MapTex"), ActiveTexture);
					MapBrush.SetResourceObject(MapDisplayMID);
				}
				else
				{
					MapBrush.SetResourceObject(ActiveTexture);
				}
			}
			else
			{
				MapBrush.SetResourceObject(ActiveTexture);
			}
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

	// ── 전장의 안개 오버레이 ────────────────────────────────────────────────────
	// RevealMask의 알파와 연무 텍스처를 UI 머티리얼에서 합성한다.
	// 맵과 동일 rect/UV를 사용하므로 줌·패닝 중에도 탐색 경계가 지형에 고정된다.
	if (bEnableFogOfWar && MapSub && MapSub->HasValidBounds())
	{
		if (UTexture2D* RevealTex = MapSub->GetRevealMaskTexture())
		{
			FSlateBrush FogBrush;
			// 머티리얼 UV를 축소하지 않고 Slate 브러시 자체를 둥글게 클리핑한다.
			// 픽셀 기준 반경이라 맵 종횡비나 확대 배율에 관계없이 모서리 곡률이 일정하다.
			FogBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
			FogBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
			FogBrush.OutlineSettings.CornerRadii = FVector4(18.0f, 18.0f, 18.0f, 18.0f);
			if (FogDisplayMaterial && FogTexture)
			{
				if (!FogDisplayMID || FogDisplayMID->Parent != FogDisplayMaterial)
				{
					FogDisplayMID = UMaterialInstanceDynamic::Create(
						FogDisplayMaterial, const_cast<URetrieveWorldMapWidget*>(this));
				}

				if (FogDisplayMID)
				{
					FogDisplayMID->SetTextureParameterValue(TEXT("FogTexA"), FogTexture);
					FogDisplayMID->SetTextureParameterValue(TEXT("FogTexB"), FogTexture);
					FogDisplayMID->SetTextureParameterValue(TEXT("RevealTex"), RevealTex);
					FogDisplayMID->SetVectorParameterValue(TEXT("FogColor"), FogColor);
					FogBrush.SetResourceObject(FogDisplayMID);
				}
			}

			// 에셋이 누락된 빌드에서도 기존 단색 마스크 표현으로 안전하게 폴백한다.
			if (!FogBrush.GetResourceObject())
			{
				FogBrush.SetResourceObject(RevealTex);
			}
			FogBrush.ImageSize = MapDrawSize;

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				++CurrentLayer,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(MapDrawSize),
					FSlateLayoutTransform(FVector2f(MapTopLeft))
				),
				&FogBrush,
				ESlateDrawEffect::None,
				FogDisplayMID ? FLinearColor::White : FogColor
			);
		}
	}

	// ── DataAsset 기반 정적 아이콘 (WP 로드 여부 완전 무관) ────────────────────
	// 화톳불 포함 모든 아이콘 항상 표시. 모닥불 타입은 활성화 여부에 따라 색상 구분.
	if (MapSub && MapSub->HasValidBounds() && WorldMapIconData)
	{
		for (const FRetrieveMapIconEntry& Entry : WorldMapIconData->Icons)
		{
			if (HiddenIconTypesOnWorldMap.Contains(Entry.IconType)) { continue; }

			const FVector2D IconUV     = MapSub->WorldToUV(Entry.WorldLocation);
			const FVector2D IconScreen = UVToScreen(IconUV, Center, ScaledW, ScaledH);

			if (!MapViewRect.ContainsPoint(FVector2D(IconScreen))) { continue; }

			// 지역명 전용 타입: 아이콘 없이 큰 폰트 라벨만 렌더한다.
			if (Entry.IconType == ERetrieveMapIconType::Region)
			{
				if (!Entry.MapLabel.IsEmpty() && Entry.bShowLabel)
				{
					const FSlateFontInfo RegionFont =
						FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt(RegionLabelFontSize * URetrieveUISettingsLibrary::GetUIScale()));
					const FLinearColor RegionColor =
						RegionLabelColor.A > KINDA_SMALL_NUMBER ? RegionLabelColor : LabelColor;
					DrawLabel(OutDrawElements, CurrentLayer, AllottedGeometry,
					          Entry.MapLabel.ToString(),
					          FVector2D(IconScreen.X, IconScreen.Y),
					          RegionFont, RegionColor);
				}
				continue;
			}

			// 전장의 안개: 아직 탐색하지 않은 영역의 아이콘은 숨긴다(지역명 라벨은 위에서 예외 처리).
			if (bEnableFogOfWar && !MapSub->IsWorldLocationExplored(Entry.WorldLocation))
			{
				continue;
			}

			FRetrieveMapIconSnapshot Snap;
			Snap.WorldLocation        = Entry.WorldLocation;
			Snap.IconType             = Entry.IconType;
			Snap.MapLabel             = Entry.MapLabel;
			Snap.bShowLabelOnWorldMap = Entry.bShowLabel;

			// 모닥불 아이콘: 활성화 여부에 따라 색상 구분
			if (Entry.IconType == ERetrieveMapIconType::Bonfire && IconRegistry)
			{
				const bool bIsActivated = IsBonfireEntryActivated(Entry);

				const FRetrieveMapIconRow& Row = IconRegistry->FindRow(Entry.IconType);
				Snap.bOverrideIcon    = true;
				Snap.OverrideTexture  = Row.IconTexture;
				Snap.OverrideColor    = bIsActivated ? BonfireActivatedColor : BonfireInactiveColor;
				Snap.OverrideSize     = Row.IconSize;
			}
			// 그 외 타입: 액터의 MapIconComponent에서 구운 개별 오버라이드 반영
			else if (Entry.bOverrideIcon)
			{
				Snap.bOverrideIcon    = true;
				Snap.OverrideTexture  = Entry.OverrideTexture;
				Snap.OverrideColor    = Entry.OverrideColor;
				Snap.OverrideSize     = Entry.OverrideSize;
			}

			DrawWorldIcon(OutDrawElements, CurrentLayer, AllottedGeometry, Snap, IconScreen);

			if (!Entry.MapLabel.IsEmpty() && Entry.bShowLabel)
			{
				float IconHalfSize = 8.0f;
				if (Snap.bOverrideIcon)
				{
					IconHalfSize = Snap.OverrideSize * 0.5f;
				}
				else if (IconRegistry)
				{
					IconHalfSize = IconRegistry->FindRow(Entry.IconType).IconSize * 0.5f;
				}
				const FSlateFontInfo IconFont =
					FCoreStyle::GetDefaultFontStyle("Regular", FMath::RoundToInt(IconLabelFontSize * URetrieveUISettingsLibrary::GetUIScale()));
				DrawLabel(OutDrawElements, CurrentLayer, AllottedGeometry,
				          Entry.MapLabel.ToString(),
				          FVector2D(IconScreen.X, IconScreen.Y + IconHalfSize + 4.0f),
				          IconFont, LabelColor);
			}
		}
	}

	// ── 사용자 웨이포인트 마커 ─────────────────────────────────────────────────
	if (MapSub && MapSub->HasValidBounds())
	{
		const TArray<FUserWaypoint>& Waypoints = MapSub->GetUserWaypoints();
		const FSlateFontInfo WpFont = FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt(10 * URetrieveUISettingsLibrary::GetUIScale()));

		for (const FUserWaypoint& WP : Waypoints)
		{
			const FVector2D WpUV     = MapSub->WorldToUV(WP.WorldLocation);
			const FVector2D WpScreen = UVToScreen(WpUV, Center, ScaledW, ScaledH);

			if (!MapViewRect.ContainsPoint(FVector2D(WpScreen))) { continue; }

			const float    Sz      = WaypointMarkerSize;
			const FVector2D HalfSz(Sz * 0.5f, Sz * 0.5f);
			const FVector2D DrawPos = WpScreen - HalfSz;

			FSlateBrush WpBrush;
			if (WaypointMarkerTexture)
			{
				WpBrush.SetResourceObject(WaypointMarkerTexture);
			}
			WpBrush.ImageSize = FVector2D(Sz, Sz);

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				++CurrentLayer,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(Sz, Sz),
					FSlateLayoutTransform(FVector2f(DrawPos))
				),
				WaypointMarkerTexture ? &WpBrush : FCoreStyle::Get().GetBrush("WhiteBrush"),
				ESlateDrawEffect::None,
				WP.Color
			);

			if (!WP.Label.IsEmpty())
			{
				DrawLabel(OutDrawElements, CurrentLayer, AllottedGeometry,
				          WP.Label.ToString(),
				          FVector2D(WpScreen.X, WpScreen.Y - HalfSz.Y - 3.0f),
				          WpFont, WP.Color);
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

			// 대비 외곽선(배킹): 마커보다 크게 어두운 색으로 먼저 그려 밝은 지형/흰 아이콘 위에서 분리한다.
			if (PlayerMarkerOutlineScale > 1.0f && PlayerMarkerOutlineColor.A > KINDA_SMALL_NUMBER)
			{
				const FVector2D OutlineSz      = MarkerSz * PlayerMarkerOutlineScale;
				const FVector2D OutlineTopLeft = PlayerScreen - OutlineSz * 0.5f;

				FSlateBrush OutlineBrush;
				OutlineBrush.SetResourceObject(PlayerMarkerTexture);
				OutlineBrush.ImageSize = OutlineSz;

				FSlateDrawElement::MakeRotatedBox(
					OutDrawElements,
					++CurrentLayer,
					AllottedGeometry.ToPaintGeometry(
						FVector2f(OutlineSz),
						FSlateLayoutTransform(FVector2f(OutlineTopLeft))
					),
					PlayerMarkerTexture ? &OutlineBrush : FCoreStyle::Get().GetBrush("WhiteBrush"),
					ESlateDrawEffect::None,
					FMath::DegreesToRadians(CameraYaw),
					TOptional<FVector2D>(AbsPlayerScreen),
					FSlateDrawElement::ERotationSpace::RelativeToWorld,
					PlayerMarkerOutlineColor
				);
			}

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
					FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt(PlayerLabelFontSize * URetrieveUISettingsLibrary::GetUIScale()));
				DrawLabel(OutDrawElements, CurrentLayer, AllottedGeometry,
				          LocationText.ToString(),
				          FVector2D(PlayerScreen.X, PlayerScreen.Y - MarkerSz.Y * 0.5f - 6.0f),
				          LabelFont, EffectiveLabelColor);
			}
		}
	}

	OutDrawElements.PopClip();
	if (GetWidgetFromName(TEXT("Panel_MapLegend")) || GetWidgetFromName(TEXT("Panel_MapControls")))
	{
		return CurrentLayer;
	}
	// Side panels stay outside the clipped map viewport and use the real map icons.
	const float UIScale = URetrieveUISettingsLibrary::GetUIScale();
	const FSlateFontInfo PanelTitleFont = FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt(15.0f * UIScale));
	const FSlateFontInfo PanelBodyFont = FCoreStyle::GetDefaultFontStyle("Regular", FMath::RoundToInt(11.0f * UIScale));
	const FSlateFontInfo PanelKeyFont = FCoreStyle::GetDefaultFontStyle("Bold", FMath::RoundToInt(10.0f * UIScale));
	const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");

	auto DrawPanel = [&](const FVector2D& Position, const FVector2D& Size)
	{
		FSlateDrawElement::MakeBox(OutDrawElements, ++CurrentLayer,
			AllottedGeometry.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(Position))),
			WhiteBrush, ESlateDrawEffect::None, FLinearColor(0.015f, 0.010f, 0.006f, 0.88f));
		FSlateDrawElement::MakeBox(OutDrawElements, ++CurrentLayer,
			AllottedGeometry.ToPaintGeometry(FVector2f(Size - FVector2D(4.0f, 4.0f)),
				FSlateLayoutTransform(FVector2f(Position + FVector2D(2.0f, 2.0f)))),
			WhiteBrush, ESlateDrawEffect::None, FLinearColor(0.10f, 0.065f, 0.035f, 0.92f));
	};

	auto DrawPanelText = [&](const FString& Text, const FVector2D& Position, const FVector2D& Size,
		const FSlateFontInfo& Font, const FLinearColor& Color)
	{
		FSlateDrawElement::MakeText(OutDrawElements, ++CurrentLayer,
			AllottedGeometry.ToPaintGeometry(FVector2f(Size), FSlateLayoutTransform(FVector2f(Position))),
			Text, Font, ESlateDrawEffect::None, Color);
	};

	auto DrawLegendIcon = [&](UTexture2D* Texture, const FLinearColor& Tint, const FVector2D& Position)
	{
		FSlateBrush IconBrush;
		if (Texture) { IconBrush.SetResourceObject(Texture); }
		IconBrush.ImageSize = FVector2D(22.0f, 22.0f);
		FSlateDrawElement::MakeBox(OutDrawElements, ++CurrentLayer,
			AllottedGeometry.ToPaintGeometry(FVector2f(22.0f, 22.0f), FSlateLayoutTransform(FVector2f(Position))),
			Texture ? &IconBrush : WhiteBrush, ESlateDrawEffect::None,
			Texture ? Tint : FLinearColor(1.0f, 0.72f, 0.22f, 1.0f));
	};

	auto GetLegendLabel = [](ERetrieveMapIconType IconType) -> const TCHAR*
	{
		switch (IconType)
		{
		case ERetrieveMapIconType::POI:         return TEXT("?? ??");
		case ERetrieveMapIconType::Lumen:       return TEXT("??");
		case ERetrieveMapIconType::Boss:        return TEXT("??");
		case ERetrieveMapIconType::Outpost:     return TEXT("????");
		case ERetrieveMapIconType::FirstWeapon: return TEXT("? ??");
		case ERetrieveMapIconType::Bonfire:     return TEXT("???");
		case ERetrieveMapIconType::Shop:        return TEXT("??");
		case ERetrieveMapIconType::Enemy:       return TEXT("?");
		default:                                return TEXT("??");
		}
	};

	TArray<ERetrieveMapIconType> LegendTypes;
	if (WorldMapIconData)
	{
		for (const FRetrieveMapIconEntry& Entry : WorldMapIconData->Icons)
		{
			if (Entry.IconType != ERetrieveMapIconType::None && Entry.IconType != ERetrieveMapIconType::Player)
			{
				LegendTypes.AddUnique(Entry.IconType);
			}
		}
	}
	if (LegendTypes.Num() == 0)
	{
		LegendTypes.Add(ERetrieveMapIconType::POI);
		LegendTypes.Add(ERetrieveMapIconType::Bonfire);
	}
	if (LegendTypes.Num() > 7) { LegendTypes.SetNum(7); }

	const float PanelTop = FMath::Max(96.0f, MapViewTopLeft.Y + 14.0f);
	const float LeftPanelWidth = FMath::Min(214.0f, MapViewTopLeft.X - 28.0f);
	const float RightPanelX = MapViewTopLeft.X + MapViewSize.X + 18.0f;
	const float RightPanelWidth = FMath::Min(224.0f, WidgetSize.X - RightPanelX - 24.0f);

	if (LeftPanelWidth >= 150.0f)
	{
		const FVector2D LeftPanelPos(14.0f, PanelTop);
		const FVector2D LeftPanelSize(LeftPanelWidth, 86.0f + 31.0f * (LegendTypes.Num() + 1));
		DrawPanel(LeftPanelPos, LeftPanelSize);
		DrawPanelText(TEXT("?? ??"), LeftPanelPos + FVector2D(16.0f, 12.0f),
			FVector2D(LeftPanelWidth - 28.0f, 24.0f), PanelTitleFont, FLinearColor(1.0f, 0.82f, 0.48f, 1.0f));

		float RowY = LeftPanelPos.Y + 45.0f;
		DrawLegendIcon(PlayerMarkerTexture, PlayerMarkerColor, FVector2D(LeftPanelPos.X + 16.0f, RowY));
		DrawPanelText(TEXT("???? ??"), FVector2D(LeftPanelPos.X + 48.0f, RowY + 3.0f),
			FVector2D(LeftPanelWidth - 60.0f, 24.0f), PanelBodyFont, FLinearColor::White);
		RowY += 31.0f;

		for (const ERetrieveMapIconType IconType : LegendTypes)
		{
			FRetrieveMapIconRow FallbackRow;
			const FRetrieveMapIconRow* Row = &FallbackRow;
			if (IconRegistry) { Row = &IconRegistry->FindRow(IconType); }
			const FLinearColor IconColor = IconType == ERetrieveMapIconType::Bonfire ? BonfireActivatedColor : Row->IconColor;
			DrawLegendIcon(Row->IconTexture, IconColor, FVector2D(LeftPanelPos.X + 16.0f, RowY));
			DrawPanelText(GetLegendLabel(IconType), FVector2D(LeftPanelPos.X + 48.0f, RowY + 3.0f),
				FVector2D(LeftPanelWidth - 60.0f, 24.0f), PanelBodyFont, FLinearColor::White);
			RowY += 31.0f;
		}
	}

	if (RightPanelWidth >= 160.0f)
	{
		const FVector2D RightPanelPos(RightPanelX, PanelTop);
		const FVector2D RightPanelSize(RightPanelWidth, 272.0f);
		DrawPanel(RightPanelPos, RightPanelSize);
		DrawPanelText(TEXT("?? ??"), RightPanelPos + FVector2D(16.0f, 12.0f),
			FVector2D(RightPanelWidth - 28.0f, 24.0f), PanelTitleFont, FLinearColor(1.0f, 0.82f, 0.48f, 1.0f));

		const FVector2D KeyPos(RightPanelPos.X + 16.0f, RightPanelPos.Y + 50.0f);
		const FVector2D TextPos(RightPanelPos.X + 70.0f, RightPanelPos.Y + 53.0f);
		const FLinearColor BodyColor(0.96f, 0.92f, 0.84f, 1.0f);

		DrawPanel(KeyPos, FVector2D(42.0f, 25.0f));
		DrawPanelText(TEXT("?"), KeyPos + FVector2D(12.0f, 3.0f), FVector2D(28.0f, 20.0f), PanelKeyFont, BodyColor);
		DrawPanelText(TEXT("?? / ??"), TextPos, FVector2D(RightPanelWidth - 84.0f, 24.0f), PanelBodyFont, BodyColor);

		DrawPanel(KeyPos + FVector2D(0.0f, 36.0f), FVector2D(42.0f, 25.0f));
		DrawPanelText(TEXT("???"), KeyPos + FVector2D(3.0f, 39.0f), FVector2D(36.0f, 20.0f), PanelKeyFont, BodyColor);
		DrawPanelText(TEXT("?? ? ???: ?? ??"), TextPos + FVector2D(0.0f, 36.0f),
			FVector2D(RightPanelWidth - 84.0f, 40.0f), PanelBodyFont, BodyColor);

		DrawPanel(KeyPos + FVector2D(0.0f, 82.0f), FVector2D(42.0f, 25.0f));
		DrawPanelText(TEXT("T"), KeyPos + FVector2D(15.0f, 85.0f), FVector2D(20.0f, 20.0f), PanelKeyFont, BodyColor);
		DrawPanelText(TEXT("?? ?? ??"), TextPos + FVector2D(0.0f, 82.0f),
			FVector2D(RightPanelWidth - 84.0f, 24.0f), PanelBodyFont, BodyColor);

		DrawLegendIcon(nullptr, BonfireActivatedColor, KeyPos + FVector2D(10.0f, 128.0f));
		DrawPanelText(TEXT("?? ??? ?? ??"), TextPos + FVector2D(0.0f, 124.0f),
			FVector2D(RightPanelWidth - 84.0f, 24.0f), PanelBodyFont, BodyColor);
		DrawPanelText(TEXT("?? ??"), TextPos + FVector2D(0.0f, 146.0f),
			FVector2D(RightPanelWidth - 84.0f, 24.0f), PanelBodyFont, BodyColor);
	}

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

	const FVector2D Center        = GetMapDrawCenter(MapViewTopLeft, MapViewSize);

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

FReply URetrieveWorldMapWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey PressedKey = InKeyEvent.GetKey();

	// T: 커서 위치에 웨이포인트 토글(근처에 있으면 삭제, 없으면 추가)
	if (PressedKey == EKeys::T)
	{
		UWorld* World = GetWorld();
		URetrieveMapSubsystem* MapSub = World ? World->GetSubsystem<URetrieveMapSubsystem>() : nullptr;
		if (!MapSub || !MapSub->HasValidBounds()) { return FReply::Unhandled(); }

		const FVector2D AbsCursorPos  = FSlateApplication::Get().GetCursorPos();
		const FVector2D LocalCursorPos = InGeometry.AbsoluteToLocal(FVector2f(AbsCursorPos));

		FVector2D MapViewTopLeft, MapViewSize;
		GetMapViewRect(InGeometry, MapViewTopLeft, MapViewSize);

		if (!IsInsideMapView(LocalCursorPos, MapViewTopLeft, MapViewSize))
		{
			return FReply::Unhandled();
		}

		const FVector2D Center = GetMapDrawCenter(MapViewTopLeft, MapViewSize);
		float BaseW = MapViewSize.X, BaseH = MapViewSize.Y;
		ComputeBaseMapSize(MapSub, MapViewSize, BaseW, BaseH);
		const float ScaledW = BaseW * ZoomLevel;
		const float ScaledH = BaseH * ZoomLevel;

		const FVector2D WaypointUV = ScreenToUV(LocalCursorPos, Center, ScaledW, ScaledH);

		// UV 범위 벗어나면 무시
		if (WaypointUV.X < 0.0f || WaypointUV.X > 1.0f ||
		    WaypointUV.Y < 0.0f || WaypointUV.Y > 1.0f)
		{
			return FReply::Unhandled();
		}

		// 커서 근처에 이미 마커가 있으면 그 마커를 삭제한다(T 토글: 같은 위치에서 다시 누르면 제거).
		// 반경은 마커 크기 기준(최소 18px)이라 마커 위에 커서를 올려 누르는 느낌으로 동작한다.
		const float RemoveRadiusPx = FMath::Max(WaypointMarkerSize, 18.0f);
		for (const FUserWaypoint& WP : MapSub->GetUserWaypoints())
		{
			const FVector2D WpScreen = UVToScreen(MapSub->WorldToUV(WP.WorldLocation), Center, ScaledW, ScaledH);
			if (FVector2D::Distance(WpScreen, LocalCursorPos) <= RemoveRadiusPx)
			{
				MapSub->RemoveUserWaypointById(WP.WaypointId);
				return FReply::Handled();
			}
		}

		const FVector WaypointWorld = MapSub->UVToWorld(WaypointUV);

		// 마커가 여러 개일 때 구분되도록 순서(현재 개수) 기반 팔레트 색을 부여한다.
		// WP.Color는 월드맵·미니맵·나침반이 공유하므로 세 곳 모두 같은 색으로 연동된다.
		static const FLinearColor WaypointPalette[] = {
			FLinearColor(1.00f, 0.45f, 0.10f), // 주황
			FLinearColor(0.20f, 0.80f, 1.00f), // 하늘
			FLinearColor(0.40f, 1.00f, 0.40f), // 연두
			FLinearColor(1.00f, 0.90f, 0.20f), // 노랑
			FLinearColor(0.90f, 0.40f, 1.00f), // 보라
			FLinearColor(1.00f, 0.30f, 0.30f), // 빨강
		};
		const FLinearColor NewColor =
			WaypointPalette[MapSub->GetUserWaypoints().Num() % UE_ARRAY_COUNT(WaypointPalette)];
		MapSub->AddUserWaypoint(WaypointWorld, FText::GetEmpty(), NewColor);
		return FReply::Handled();
	}

	// Delete: 웨이포인트 전체 삭제
	if (PressedKey == EKeys::Delete)
	{
		UWorld* World = GetWorld();
		if (URetrieveMapSubsystem* MapSub = World ? World->GetSubsystem<URetrieveMapSubsystem>() : nullptr)
		{
			MapSub->ClearUserWaypoints();
		}
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

// ---------- 헬퍼 ----------

FVector2D URetrieveWorldMapWidget::GetMapDrawCenter(const FVector2D& MapViewTopLeft, const FVector2D& MapViewSize) const
{
	// 텍스처·아이콘 공통 기준점. MapTextureOffset을 여기 한 곳에서 더해 항상 함께 이동한다.
	return MapViewTopLeft + MapViewSize * 0.5f + MapTextureOffset;
}

void URetrieveWorldMapWidget::GetMapViewRect(const FGeometry& AllottedGeometry, FVector2D& OutTopLeft, FVector2D& OutSize) const
{
	const FVector2D WidgetSize = AllottedGeometry.GetLocalSize();

	// ── 1차: 실제 바인딩된 MapViewport 위젯 geometry (해상도/DPI 자동 적응) ──
	// MapViewport는 WBP에서 CanvasPanel_Main 안에 풀 CanvasPanel로 바인딩(is_variable)돼
	// 그 자체가 정확한 뷰포트 인셋(현재 오프셋 240/84/240/54)을 갖는다.
	// cached geometry를 AllottedGeometry 로컬 좌표로 변환해 그대로 채택하고, 위젯 경계와의
	// 교집합으로만 클램프한다.
	//
	// 과거에는 여기서 "크기 70% 이상 + 중심 25% 이내"라는 휴리스틱으로 이 geometry를
	// 검증한 뒤에만 채택하고, 실패 시 (앵커% + 절대픽셀 패딩) 폴백 rect를 썼다. 그런데
	// 폴백 패딩(98/117px)이 실제 뷰포트 인셋(240px)과 크게 달라, 1280×720·울트라와이드 등
	// 일부 해상도에서 정상 geometry가 오탐되어 폴백으로 넘어가면 맵이 실제 뷰포트보다
	// 넓게/치우쳐 그려지며 영역 밖으로 오버플로되는 버그가 있었다(#3). → 휴리스틱 게이트 제거.
	if (MapViewport)
	{
		const FGeometry& ViewGeometry = MapViewport->GetCachedGeometry();
		const FVector2D ViewSize = ViewGeometry.GetLocalSize();
		if (ViewSize.X > 1.0f && ViewSize.Y > 1.0f)
		{
			const FVector2D AbsTopLeft = FVector2D(ViewGeometry.LocalToAbsolute(FVector2f::ZeroVector));
			const FVector2D AbsBottomRight = FVector2D(ViewGeometry.LocalToAbsolute(FVector2f(ViewSize)));
			const FVector2D CandidateTopLeft = AllottedGeometry.AbsoluteToLocal(AbsTopLeft);
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

			// #312에서 이 geometry를 "무조건 채택"으로 바꾼 뒤, 열기 애니메이션의 렌더 트랜스폼이
			// 페인트 직전 MapViewport에 남긴 축소·오프셋 geometry(예: 우하단으로 ~0.3배 축소, 위치는
			// 해상도/창모드에 따라 달라짐)까지 그대로 채택돼 맵이 패널이 아닌 엉뚱한 곳에 그려지는
			// 회귀가 있었다. 해상도 무관한 상대 기준으로 "명백히 깨진" geometry만 걸러내고 폴백한다.
			// (과거 게이트의 70%/25% 임계는 1280×720 등에서 정상 geometry를 오탐 → 임계를 넉넉히 완화.)
			const FVector2D CandCenter(
				(CandidateRect.Left + CandidateRect.Right) * 0.5f,
				(CandidateRect.Top + CandidateRect.Bottom) * 0.5f);
			const FVector2D WidgetCenter(WidgetSize.X * 0.5f, WidgetSize.Y * 0.5f);
			const bool bCoversEnough =
				CandidateW >= WidgetSize.X * 0.45f && CandidateH >= WidgetSize.Y * 0.45f;
			const bool bCentered =
				FMath::Abs(CandCenter.X - WidgetCenter.X) <= WidgetSize.X * 0.25f &&
				FMath::Abs(CandCenter.Y - WidgetCenter.Y) <= WidgetSize.Y * 0.25f;
			if (CandidateW > 1.0f && CandidateH > 1.0f && bCoversEnough && bCentered)
			{
				OutTopLeft = FVector2D(CandidateRect.Left, CandidateRect.Top);
				OutSize = FVector2D(CandidateW, CandidateH);
				LogMapViewRect(TEXT("MapViewport"), WidgetSize, OutTopLeft, OutSize);
				return;
			}
		}
	}

	// ── 폴백: MapViewport geometry가 아직 준비 안 된 첫 프레임 등에서만 사용 ──
	// (앵커% + 절대픽셀 패딩). 다음 프레임에 위 1차 경로가 실제 geometry로 교정한다.
	// 크기 하한을 위젯의 10%로 둬 어떤 해상도에서도 rect가 붕괴하지 않게 한다.
	const float BW_Left   = WidgetSize.X * MapWindowAnchorMin.X;
	const float BW_Top    = WidgetSize.Y * MapWindowAnchorMin.Y;
	const float BW_Right  = WidgetSize.X * MapWindowAnchorMax.X;
	const float BW_Bottom = WidgetSize.Y * MapWindowAnchorMax.Y;

	OutTopLeft = FVector2D(
		BW_Left + MapViewportPadding.Left,
		BW_Top  + MapViewportPadding.Top);
	OutSize = FVector2D(
		FMath::Max(WidgetSize.X * 0.1f, (BW_Right  - BW_Left) - MapViewportPadding.Left - MapViewportPadding.Right),
		FMath::Max(WidgetSize.Y * 0.1f, (BW_Bottom - BW_Top)  - MapViewportPadding.Top  - MapViewportPadding.Bottom));
	LogMapViewRect(TEXT("Fallback(Anchor+Padding)"), WidgetSize, OutTopLeft, OutSize);
}

void URetrieveWorldMapWidget::LogMapViewRect(const TCHAR* PathName, const FVector2D& WidgetSize,
                                             const FVector2D& TopLeft, const FVector2D& Size) const
{
	// 해상도 변경/경로 전환 등 값이 의미 있게 바뀐 경우에만 로깅 — 매 프레임 스팸 방지.
	const bool bChanged =
		DbgLastRectPath != PathName
		|| !DbgLastRectTopLeft.Equals(TopLeft, 0.5f)
		|| !DbgLastRectSize.Equals(Size, 0.5f);
	if (!bChanged) { return; }

	DbgLastRectPath    = PathName;
	DbgLastRectTopLeft = TopLeft;
	DbgLastRectSize    = Size;

	UE_LOG(LogTemp, Log,
		TEXT("[WorldMap] GetMapViewRect path=%s Widget=(%.0f, %.0f) TopLeft=(%.1f, %.1f) Size=(%.1f, %.1f)"),
		PathName, WidgetSize.X, WidgetSize.Y, TopLeft.X, TopLeft.Y, Size.X, Size.Y);
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

FVector2D URetrieveWorldMapWidget::ScreenToUV(
	const FVector2D& ScreenPos, const FVector2D& Center,
	float ScaledW, float ScaledH) const
{
	return ViewCenterUV + FVector2D(
		(ScreenPos.X - Center.X) / FMath::Max(ScaledW, 1.0f),
		(ScreenPos.Y - Center.Y) / FMath::Max(ScaledH, 1.0f)
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

	// 대비 외곽선(배킹): 아이콘보다 크게 어두운 색으로 먼저 그려 밝은 지형/겹친 아이콘과 분리한다.
	if (bDrawIconOutline && IconOutlineScale > 1.0f && IconOutlineColor.A > KINDA_SMALL_NUMBER)
	{
		const FVector2D OutlineSz  = IconSz * IconOutlineScale;
		const FVector2D OutlinePos = ScreenPos - OutlineSz * 0.5f;

		FSlateBrush OutlineBrush;
		OutlineBrush.SetResourceObject(Texture);
		OutlineBrush.ImageSize = OutlineSz;

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(
				FVector2f(OutlineSz),
				FSlateLayoutTransform(FVector2f(OutlinePos))
			),
			Texture ? &OutlineBrush : FCoreStyle::Get().GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			IconOutlineColor
		);
	}

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
	if (!SaveSub) { return false; }

	UWorld* World = GetWorld();
	URetrieveMapSubsystem* MapSub = World ? World->GetSubsystem<URetrieveMapSubsystem>() : nullptr;
	if (!MapSub || !MapSub->HasValidBounds()) { return false; }

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

		if (Entry.BonfireId.IsNone() || !SaveSub->IsBonfireActivated(Entry.BonfireId))
		{
			continue;
		}

		bDoubleClickConsumed = true;
		LastClickedBonfireId = NAME_None;
		LastBonfireIconClickTime = -1.0;

		const FText DisplayName = Entry.MapLabel;
		UE_LOG(LogTemp, Log,
			TEXT("[WorldMap] Bonfire double click (unloaded) - BonfireId=%s"),
			*Entry.BonfireId.ToString());

		APlayerController* PC = GetWorldMapPlayerController();
		if (!PC || !FastTravelDialogClass) { return false; }

		CloseActiveFastTravelDialog();
		UUserWidget* Dialog = CreateWidget<UUserWidget>(PC, FastTravelDialogClass);
		if (!Dialog) { return false; }

		struct FInitParams { FName BonfireId; FText BonfireDisplayName; };
		FInitParams Params{ Entry.BonfireId, DisplayName };
		if (UFunction* Fn = Dialog->FindFunction(TEXT("InitDialog")))
		{
			Dialog->ProcessEvent(Fn, &Params);
		}
		ActiveFastTravelDialog   = Dialog;
		ActiveFastTravelBonfireId = Entry.BonfireId;
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

	return false;
}

bool URetrieveWorldMapWidget::IsBonfireEntryActivated(const FRetrieveMapIconEntry& Entry) const
{
	if (Entry.IconType != ERetrieveMapIconType::Bonfire)
	{
		return false;
	}

	// 에디터에서 활성으로 배치된 모닥불은 액터/세이브와 무관하게 항상 활성으로 표시.
	if (Entry.bStartActivated)
	{
		return true;
	}

	const UGameInstance* GI = GetGameInstance();
	const URetrieveSaveSubsystem* SaveSub = GI ? GI->GetSubsystem<URetrieveSaveSubsystem>() : nullptr;
	if (SaveSub && !Entry.BonfireId.IsNone() && SaveSub->IsBonfireActivated(Entry.BonfireId))
	{
		return true;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const float MatchRadiusSq = FMath::Square(FMath::Max(BonfireActivationCheckRadius, 50.0f));
	for (TActorIterator<ARetrieveBonfireActor> It(World); It; ++It)
	{
		const ARetrieveBonfireActor* Bonfire = *It;
		if (!IsValid(Bonfire) || !Bonfire->IsActivated())
		{
			continue;
		}

		if (!Entry.BonfireId.IsNone() && Bonfire->BonfireId == Entry.BonfireId)
		{
			return true;
		}

		if (FVector::DistSquared(Bonfire->GetActorLocation(), Entry.WorldLocation) <= MatchRadiusSq)
		{
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
	if (!PC || !FastTravelDialogClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[WorldMap] 빠른 이동 확인 창 생성 실패 — PC=%s DialogClass=%s"),
			PC ? TEXT("Valid") : TEXT("None"),
			FastTravelDialogClass ? *FastTravelDialogClass->GetName() : TEXT("None"));
		return false;
	}

	UUserWidget* Dialog = CreateWidget<UUserWidget>(PC, FastTravelDialogClass);
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
			*FastTravelDialogClass->GetName());
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

	if (!FastTravelLoadingClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WorldMap] Fast travel loading overlay class not assigned"));
		return;
	}

	ActiveFastTravelLoadingOverlay = CreateWidget<UUserWidget>(PC, FastTravelLoadingClass);
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
