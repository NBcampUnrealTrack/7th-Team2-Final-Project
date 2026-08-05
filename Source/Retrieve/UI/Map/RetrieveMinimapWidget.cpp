#include "UI/Map/RetrieveMinimapWidget.h"
#include "Subsystems/RetrieveMapSubsystem.h"
#include "Subsystems/RetrieveObjectiveMarkerSubsystem.h"
#include "Subsystems/RetrieveGuidanceSubsystem.h"
#include "Engine/GameInstance.h"
#include "Fonts/FontMeasure.h"
#include "UI/RetrieveUISettingsLibrary.h"
#include "Components/World/RetrieveMapIconComponent.h"
#include "Data/RetrieveMapIconRegistry.h"
#include "World/RetrieveBonfireActor.h"
#include "Save/RetrieveSaveSubsystem.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rendering/DrawElements.h"
#include "SlateMaterialBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Data/RetrieveMapConfigDataAsset.h"

namespace
{
	static UTexture* ResolveMinimapTexture(const URetrieveMinimapWidget* Widget, const URetrieveMapSubsystem* MapSub)
	{
		if (!Widget)
		{
			return nullptr;
		}

		if (Widget->BakedMapTexture)
		{
			return Widget->BakedMapTexture;
		}

		if (MapSub && MapSub->MapConfig && MapSub->MapConfig->BakedMapTexture)
		{
			return MapSub->MapConfig->BakedMapTexture;
		}

		if (MapSub && MapSub->BakedMapTexture)
		{
			return MapSub->BakedMapTexture;
		}

		return nullptr;
	}
}

void URetrieveMinimapWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	HideSquareMinimapBackground();
}

void URetrieveMinimapWidget::NativeConstruct()
{
	Super::NativeConstruct();

	HideSquareMinimapBackground();

	UWorld* World = GetWorld();
	URetrieveMapSubsystem* MapSub = World ? World->GetSubsystem<URetrieveMapSubsystem>() : nullptr;

	// 월드맵을 열기 전에도 미니맵이 독립적으로 올바른 MapConfig / Bounds / Texture를 갖도록 한다.
	// 기존에는 NativeTick에서 EnsureMapConfigLoaded()를 호출했기 때문에 첫 Paint 시점에
	// 폴백 Bounds 또는 빈 Texture로 먼저 그려질 수 있었다.
	if (MapSub)
	{
		MapSub->InitializeMapConfigRuntime(nullptr);

		if (!IconRegistry && MapSub->MapConfig)
		{
			IconRegistry = MapSub->MapConfig->IconRegistry;
		}
	}

	if (!Image_Minimap)
	{
		return;
	}

	UMaterialInterface* BaseMaterial = MinimapMaterial;
	if (!BaseMaterial)
	{
		BaseMaterial = Cast<UMaterialInterface>(Image_Minimap->GetBrush().GetResourceObject());
	}

	if (!BaseMaterial)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Minimap] NativeConstruct: MinimapMaterial 없음"));
		return;
	}

	MinimapMID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	if (!MinimapMID)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Minimap] NativeConstruct: MID 생성 실패"));
		return;
	}

	Image_Minimap->SetBrushFromMaterial(MinimapMID);

	UTexture* InitTex = ResolveMinimapTexture(this, MapSub);
	if (InitTex)
	{
		MinimapMID->SetTextureParameterValue(TEXT("MapTexture"), InitTex);
		CachedMIDTexture = InitTex;
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Minimap] NativeConstruct: 초기 MapTexture 없음"));
	}

	// 첫 프레임부터 CenterUV/Zoom이 기본값으로 그려지는 것을 방지한다.
	const APlayerController* PC = GetOwningPlayer();
	if (MapSub && MapSub->HasValidBounds() && PC && PC->GetPawn())
	{
		UpdateMinimapMaterial(MapSub, PC->GetPawn()->GetActorLocation());
	}
}

void URetrieveMinimapWidget::HideSquareMinimapBackground()
{
	if (Image_Minimap)
	{
		Image_Minimap->SetColorAndOpacity(FLinearColor::Transparent);
		Image_Minimap->SetBrushTintColor(FSlateColor(FLinearColor::Transparent));
		Image_Minimap->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (Border_MinimapFrame)
	{
		FSlateBrush TransparentBrush;
		TransparentBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
		TransparentBrush.TintColor = FSlateColor(FLinearColor::Transparent);
		Border_MinimapFrame->SetBrush(TransparentBrush);
		Border_MinimapFrame->SetBrushColor(FLinearColor::Transparent);
	}
}

void URetrieveMinimapWidget::DrawImageBrush(
	FSlateWindowElementList& OutDrawElements,
	int32& LayerId,
	const FGeometry& AllottedGeometry,
	const UImage* Image,
	const FVector2D& Position,
	const FVector2D& Size
) const
{
	if (!Image || Size.X <= 0.0f || Size.Y <= 0.0f)
	{
		return;
	}

	const FSlateBrush& Brush = Image->GetBrush();
	if (Brush.DrawAs == ESlateBrushDrawType::NoDrawType)
	{
		return;
	}

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		++LayerId,
		AllottedGeometry.ToPaintGeometry(
			FVector2f(Size),
			FSlateLayoutTransform(FVector2f(Position))
		),
		&Brush,
		ESlateDrawEffect::None,
		Image->GetColorAndOpacity()
	);
}

void URetrieveMinimapWidget::DrawMinimapDecorations(
	FSlateWindowElementList& OutDrawElements,
	int32& LayerId,
	const FGeometry& AllottedGeometry
) const
{
	const FVector2D WidgetSize = AllottedGeometry.GetLocalSize();

	DrawImageBrush(
		OutDrawElements,
		LayerId,
		AllottedGeometry,
		IMG_Frame,
		FVector2D(-28.0f, -32.0f),
		WidgetSize + FVector2D(56.7f, 65.1f));

	DrawImageBrush(
		OutDrawElements,
		LayerId,
		AllottedGeometry,
		IMG_Tracery,
		FVector2D(0.0f, -5.0f),
		WidgetSize + FVector2D(0.0f, 10.0f));

	DrawImageBrush(
		OutDrawElements,
		LayerId,
		AllottedGeometry,
		IMG_Curlicue_Top,
		FVector2D(WidgetSize.X * 0.5f - 40.0f, -29.0f),
		FVector2D(80.0f, 42.0f));

	DrawImageBrush(
		OutDrawElements,
		LayerId,
		AllottedGeometry,
		IMG_Curlicue_Bottom,
		FVector2D(WidgetSize.X * 0.5f - 47.5f, WidgetSize.Y - 17.5f),
		FVector2D(95.0f, 35.0f));
}

void URetrieveMinimapWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UWorld* World = GetWorld();
	const APlayerController* PC = GetOwningPlayer();
	if (!World || !PC || !PC->GetPawn())
	{
		return;
	}

	URetrieveMapSubsystem* MapSub = World->GetSubsystem<URetrieveMapSubsystem>();
	if (!MapSub)
	{
		return;
	}

	// 월드맵 의존 제거: 미니맵도 매 Tick 최소 1회 이상 Config 유효성을 보장한다.
	// NativeConstruct보다 Subsystem 초기화가 늦은 경우를 위한 안전망이다.
	MapSub->EnsureMapConfigLoaded();

	if (!IconRegistry && MapSub->MapConfig)
	{
		IconRegistry = MapSub->MapConfig->IconRegistry;
	}

	if (!MapSub->HasValidBounds())
	{
		return;
	}

	const FVector PlayerLocation = PC->GetPawn()->GetActorLocation();

	// 전장의 안개: 플레이어 주변을 주기적으로 공개하고 dirty면 GPU 텍스처에 반영한다.
	const UGameInstance* GI = GetGameInstance();
	const URetrieveSaveSubsystem* SaveSub =
		GI ? GI->GetSubsystem<URetrieveSaveSubsystem>() : nullptr;
	const bool bCanRecordExploration = !SaveSub || !SaveSub->IsApplyingSave();

	if (bEnableFogOfWar && bCanRecordExploration)
	{
		FogRevealAccum += InDeltaTime;
		if (FogRevealAccum >= FogRevealInterval)
		{
			FogRevealAccum = 0.0f;
			MapSub->MarkExploredAtWorld(PlayerLocation);
			MapSub->FlushRevealMaskToTexture();
		}
	}

	if (MinimapMID)
	{
		UpdateMinimapMaterial(MapSub, PlayerLocation);
	}

	if (Image_Minimap)
	{
		FWidgetTransform Transform;
		if (RotationMode == ERetrieveMinimapRotationMode::PlayerUp)
		{
			const float CameraYaw = GetCameraYaw(PC);
			Transform.Angle = -CameraYaw;
			Transform.Scale = FVector2D(FMath::Sqrt(2.0f), FMath::Sqrt(2.0f));
		}
		Image_Minimap->SetRenderTransform(Transform);
	}
}

void URetrieveMinimapWidget::UpdateMinimapMaterial(
	URetrieveMapSubsystem* MapSub,
	const FVector& PlayerLocation
)
{
	if (!MinimapMID || !MapSub)
	{
		return;
	}

	// Subsystem 초기화 순서가 늦는 경우에도 여기에서 한 번 더 보장한다.
	MapSub->EnsureMapConfigLoaded();
	
	ActiveContext = MapSub->ResolveMinimapContext(PlayerLocation, ViewWorldRadius);
	if (ActiveContext.DisplayMode == ERetrieveMinimapDisplayMode::WorldMap && BakedMapTexture)
	{
		ActiveContext.Texture = BakedMapTexture;
	}

	if (ActiveContext.DisplayMode == ERetrieveMinimapDisplayMode::LiveRenderTarget &&
		!ActiveContext.Texture && ActiveContext.SourceArea)
	{
		MapSub->RequestIndoorCapture(ActiveContext.SourceArea);
	}

	const FVector2D PlayerUV = ActiveContext.WorldToUV(PlayerLocation);
	const float Zoom = ActiveContext.GetZoom();

	MinimapMID->SetVectorParameterValue(
		TEXT("CenterUV"),
		FLinearColor(PlayerUV.X, PlayerUV.Y, 0.0f, 0.0f)
	);
	MinimapMID->SetScalarParameterValue(TEXT("Zoom"), Zoom);

	UTexture* ActiveTexture = ActiveContext.Texture;
	if (!ActiveTexture && ActiveContext.DisplayMode == ERetrieveMinimapDisplayMode::WorldMap)
	{
		ActiveTexture = ResolveMinimapTexture(this, MapSub);
	}
	if (ActiveTexture && ActiveTexture != CachedMIDTexture)
	{
		MinimapMID->SetTextureParameterValue(TEXT("MapTexture"), ActiveTexture);
		CachedMIDTexture = ActiveTexture;
	}

	// ── 전장의 안개 파라미터 ────────────────────────────────────────────────
	// M_Minimap 그래프에서: 최종 맵 색상 = lerp(FogColor, MapColor, RevealMask.Sample(finalUV).a)
	// (RevealMask는 BGRA8, A=안개 불투명도이므로 explored = 1 - A. 실내 컨텍스트에서는 안개를 끈다.)
	MinimapMID->SetScalarParameterValue(TEXT("FogEnabled"),
		(bEnableFogOfWar && !ActiveContext.IsIndoor()) ? 1.0f : 0.0f);
	MinimapMID->SetVectorParameterValue(TEXT("FogColor"), FogColor);
	if (bEnableFogOfWar && !ActiveContext.IsIndoor())
	{
		if (UTexture2D* RevealTex = MapSub->GetRevealMaskTexture())
		{
			MinimapMID->SetTextureParameterValue(TEXT("RevealMask"), RevealTex);
		}
	}
}

void URetrieveMinimapWidget::DrawBlackCircularMap(
	FSlateWindowElementList& OutDrawElements,
	int32& LayerId,
	const FGeometry& AllottedGeometry
) const
{
	const FVector2D WidgetSize = AllottedGeometry.GetLocalSize();
	const FVector2D Center = WidgetSize * 0.5f;
	const float Radius = FMath::Min(WidgetSize.X, WidgetSize.Y) * MapCircleRadiusRatio;
	const FVector2D Diameter(Radius * 2.0f, Radius * 2.0f);
	const FVector2D Position = Center - Diameter * 0.5f;
	const FSlateRoundedBoxBrush BlackBrush(FLinearColor::Black, FVector4(Radius));

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		++LayerId,
		AllottedGeometry.ToPaintGeometry(
			FVector2f(Diameter),
			FSlateLayoutTransform(FVector2f(Position))),
		&BlackBrush,
		ESlateDrawEffect::None,
		FLinearColor::White);
}

void URetrieveMinimapWidget::DrawCircularMap(
	FSlateWindowElementList& OutDrawElements,
	int32& LayerId,
	const FGeometry& AllottedGeometry,
	float CameraYaw
) const
{
	if (!MinimapMID)
	{
		return;
	}

	const FVector2D WidgetSize = AllottedGeometry.GetLocalSize();
	const FVector2D Center = WidgetSize * 0.5f;
	const float Radius = FMath::Min(WidgetSize.X, WidgetSize.Y) * MapCircleRadiusRatio;
	const int32 SegmentCount = FMath::Clamp(MapCircleSegments, 12, 128);

	FSlateMaterialBrush MapBrush(*MinimapMID, FVector2f(WidgetSize));
	const FSlateResourceHandle Handle = MapBrush.GetRenderingResource();
	if (!Handle.IsValid())
	{
		return;
	}

	TArray<FSlateVertex> Verts;
	Verts.Reserve(SegmentCount + 1);

	const FVector2D AbsCenter = FVector2D(AllottedGeometry.LocalToAbsolute(FVector2f(Center)));
	Verts.AddZeroed();
	{
		FSlateVertex& Vert = Verts.Last();
		Vert.Position[0] = AbsCenter.X;
		Vert.Position[1] = AbsCenter.Y;
		Vert.TexCoords[0] = 0.5f;
		Vert.TexCoords[1] = 0.5f;
		Vert.TexCoords[2] = Vert.TexCoords[3] = 1.0f;
		Vert.Color = FColor::White;
	}

	const float UVRotation = RotationMode == ERetrieveMinimapRotationMode::PlayerUp ? CameraYaw : 0.0f;
	for (int32 Index = 0; Index < SegmentCount; ++Index)
	{
		const float Angle = (2.0f * PI * Index) / SegmentCount;
		const FVector2D EdgeDir(FMath::Cos(Angle), FMath::Sin(Angle));
		const FVector2D LocalPos = Center + EdgeDir * Radius;
		const FVector2D AbsPos = FVector2D(AllottedGeometry.LocalToAbsolute(FVector2f(LocalPos)));
		const FVector2D UVDir = Rotate2D(EdgeDir, UVRotation);

		Verts.AddZeroed();
		FSlateVertex& Vert = Verts.Last();
		Vert.Position[0] = AbsPos.X;
		Vert.Position[1] = AbsPos.Y;
		Vert.TexCoords[0] = 0.5f + UVDir.X * 0.5f;
		Vert.TexCoords[1] = 0.5f + UVDir.Y * 0.5f;
		Vert.TexCoords[2] = Vert.TexCoords[3] = 1.0f;
		Vert.Color = FColor::White;
	}

	TArray<SlateIndex> Indices;
	Indices.Reserve(SegmentCount * 3);
	for (int32 Index = 1; Index <= SegmentCount; ++Index)
	{
		Indices.Add(0);
		Indices.Add(Index);
		Indices.Add(Index == SegmentCount ? 1 : Index + 1);
	}

	FSlateDrawElement::MakeCustomVerts(
		OutDrawElements,
		++LayerId,
		Handle,
		Verts,
		Indices,
		nullptr,
		0,
		0);
}

int32 URetrieveMinimapWidget::NativePaint(
	const FPaintArgs& Args,
	const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements,
	int32 LayerId,
	const FWidgetStyle& InWidgetStyle,
	bool bParentEnabled
) const
{
	const FVector2D WidgetSize = AllottedGeometry.GetLocalSize();
	if (WidgetSize.X <= 1.0f || WidgetSize.Y <= 1.0f)
	{
		return Super::NativePaint(
			Args, AllottedGeometry, MyCullingRect, OutDrawElements,
			LayerId, InWidgetStyle, bParentEnabled);
	}

	const APlayerController* PC = GetOwningPlayer();
	if (!PC || !PC->GetPawn())
	{
		return Super::NativePaint(
			Args, AllottedGeometry, MyCullingRect, OutDrawElements,
			LayerId, InWidgetStyle, bParentEnabled);
	}

	const FVector2D Center        = WidgetSize * 0.5f;
	const float     MiniMapRadius = FMath::Min(WidgetSize.X, WidgetSize.Y) * MapCircleRadiusRatio;
	const FVector   PlayerLoc     = PC->GetPawn()->GetActorLocation();
	const float     CameraYaw     = GetCameraYaw(PC);
	UWorld* World = GetWorld();
	URetrieveMapSubsystem* MapSub = World ? World->GetSubsystem<URetrieveMapSubsystem>() : nullptr;
	int32 CurrentLayer = LayerId;

	// Paint가 Tick보다 먼저 호출되는 프레임에서도 Config를 보장한다.
	if (MapSub)
	{
		MapSub->EnsureMapConfigLoaded();
	}

	FRetrieveMinimapContext PaintContext = ActiveContext;
	if (MapSub)
	{
		PaintContext = MapSub->ResolveMinimapContext(PlayerLoc, ViewWorldRadius);
	}
	const bool bDrawBlack = PaintContext.DisplayMode == ERetrieveMinimapDisplayMode::Black ||
		((PaintContext.DisplayMode == ERetrieveMinimapDisplayMode::BakedTexture ||
		  PaintContext.DisplayMode == ERetrieveMinimapDisplayMode::LiveRenderTarget) &&
		 !PaintContext.HasDrawableTexture());
	if (bDrawBlack)
	{
		DrawBlackCircularMap(OutDrawElements, CurrentLayer, AllottedGeometry);
	}
	else
	{
		DrawCircularMap(OutDrawElements, CurrentLayer, AllottedGeometry, CameraYaw);
	}

	CurrentLayer = Super::NativePaint(
		Args, AllottedGeometry, MyCullingRect, OutDrawElements,
		CurrentLayer, InWidgetStyle, bParentEnabled
	);

	const FVector2D AbsCenter = FVector2D(AllottedGeometry.LocalToAbsolute(FVector2f(Center)));

	const FSlateClippingZone ClipZone(AllottedGeometry.GetLayoutBoundingRect());
	OutDrawElements.PushClip(ClipZone);

	// 에너미/아이콘 마커 (라이브 MapIconComponent) — 원형 반경 안으로 컬링.
	if (World && MapSub && MapSub->HasValidBounds() && PaintContext.bShowIcons)
	{
		for (const URetrieveMapIconComponent* Icon : MapSub->GetIcons())
		{
			if (!IsValid(Icon) || !IsValid(Icon->GetOwner()) || !Icon->bShowOnMinimap)
			{
				continue;
			}

			const FVector IconWorld = Icon->GetOwner()->GetActorLocation();
			if (!PaintContext.ContainsZ(IconWorld.Z) ||
				FVector::Dist2D(PlayerLoc, IconWorld) > PaintContext.ViewWorldRadius)
			{
				continue;
			}

			const FVector2D IconPos = WorldToLocal(
				IconWorld, PlayerLoc, Center, WidgetSize, CameraYaw,
				PaintContext.ViewWorldRadius
			);

			if (FVector2D::Distance(IconPos, Center) > MiniMapRadius)
			{
				continue;
			}

			DrawIcon(OutDrawElements, CurrentLayer, AllottedGeometry, Icon, IconPos);
		}
	}

	// 사용자 웨이포인트 마커 — 원형 반경 안으로 컬링.
	if (MapSub && PaintContext.bShowWaypoints)
	{
		for (const FUserWaypoint& WP : MapSub->GetUserWaypoints())
		{
			if (!PaintContext.ContainsZ(WP.WorldLocation.Z))
			{
				continue;
			}

			const FVector2D WpPos = WorldToLocal(
				WP.WorldLocation, PlayerLoc, Center, WidgetSize, CameraYaw,
				PaintContext.ViewWorldRadius
			);

			if (FVector2D::Distance(WpPos, Center) > MiniMapRadius)
			{
				continue;
			}

			const FVector2D WpSz(WaypointMarkerSize, WaypointMarkerSize);
			const FVector2D WpDrawPos = WpPos - WpSz * 0.5f;

			FSlateBrush WpBrush;
			if (WaypointMarkerTexture)
			{
				WpBrush.SetResourceObject(WaypointMarkerTexture);
				WpBrush.ImageSize = WpSz;
			}

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				++CurrentLayer,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(WpSz),
					FSlateLayoutTransform(FVector2f(WpDrawPos))
				),
				WaypointMarkerTexture ? &WpBrush : FCoreStyle::Get().GetBrush("WhiteBrush"),
				ESlateDrawEffect::None,
				WaypointMarkerColor
			);
		}
	}

	// ── 퀘스트 목표 마커 ─────────────────────────────────────────────────────
	// 나침반이 방위만 알려주는 것과 달리, 미니맵은 "어느 쪽으로 돌아가야 하는지"까지 보여준다.
	if (const URetrieveObjectiveMarkerSubsystem* MarkerSub =
		GetWorld() ? GetWorld()->GetSubsystem<URetrieveObjectiveMarkerSubsystem>() : nullptr)
	{
		for (const FRetrieveObjectiveMarker& Marker : MarkerSub->GetMarkers())
		{
			if (!Marker.State.bVisible)
			{
				continue;
			}
			// 월드맵과 같은 규칙: 한 번 발견한 의뢰는 계속, 메인은 항상.
			if (!MarkerSub->PassesMapVisibility(Marker))
			{
				continue;
			}

			const FVector2D ObjPos = WorldToLocal(
				Marker.State.WorldLocation, PlayerLoc, Center, WidgetSize, CameraYaw,
				PaintContext.ViewWorldRadius
			);

			if (FVector2D::Distance(ObjPos, Center) > MiniMapRadius)
			{
				continue; // 미니맵 반경 밖 — 방향은 나침반이 맡는다.
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

			const FVector2D ObjSz(ObjectiveMarkerSize, ObjectiveMarkerSize);
			const FVector2D ObjDrawPos = ObjPos - ObjSz * 0.5f;

			FSlateBrush ObjBrush;
			if (WaypointMarkerTexture)
			{
				ObjBrush.SetResourceObject(WaypointMarkerTexture);
				ObjBrush.ImageSize = ObjSz;
			}

			FSlateDrawElement::MakeBox(
				OutDrawElements,
				++CurrentLayer,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(ObjSz),
					FSlateLayoutTransform(FVector2f(ObjDrawPos))
				),
				WaypointMarkerTexture ? &ObjBrush : FCoreStyle::Get().GetBrush("WhiteBrush"),
				ESlateDrawEffect::None,
				MarkerCol
			);
		}
	}

	// 플레이어 마커 (항상 중앙, NorthUp 모드에서 카메라 방향으로 회전).
	if (PaintContext.bShowPlayerMarker)
	{
		const float MarkerRot = (RotationMode == ERetrieveMinimapRotationMode::NorthUp) ? CameraYaw : 0.0f;

		const FVector2D MarkerSz(PlayerMarkerSize, PlayerMarkerSize);
		const FVector2D MarkerPos = Center - MarkerSz * 0.5f;

		FSlateBrush Brush;
		Brush.SetResourceObject(PlayerMarkerTexture);
		Brush.ImageSize = MarkerSz;

		// 대비 외곽선(배킹): 마커보다 크게 어두운 색으로 먼저 그려 지형/아이콘 위에서 분리한다.
		if (PlayerMarkerOutlineScale > 1.0f && PlayerMarkerOutlineColor.A > KINDA_SMALL_NUMBER)
		{
			const FVector2D OutlineSz  = MarkerSz * PlayerMarkerOutlineScale;
			const FVector2D OutlinePos = Center - OutlineSz * 0.5f;

			FSlateBrush OutlineBrush;
			OutlineBrush.SetResourceObject(PlayerMarkerTexture);
			OutlineBrush.ImageSize = OutlineSz;

			FSlateDrawElement::MakeRotatedBox(
				OutDrawElements,
				++CurrentLayer,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(OutlineSz),
					FSlateLayoutTransform(FVector2f(OutlinePos))
				),
				PlayerMarkerTexture ? &OutlineBrush : FCoreStyle::Get().GetBrush("WhiteBrush"),
				ESlateDrawEffect::None,
				FMath::DegreesToRadians(MarkerRot),
				TOptional<FVector2D>(AbsCenter),
				FSlateDrawElement::ERotationSpace::RelativeToWorld,
				PlayerMarkerOutlineColor
			);
		}

		FSlateDrawElement::MakeRotatedBox(
			OutDrawElements,
			++CurrentLayer,
			AllottedGeometry.ToPaintGeometry(
				FVector2f(MarkerSz),
				FSlateLayoutTransform(FVector2f(MarkerPos))
			),
			PlayerMarkerTexture ? &Brush : FCoreStyle::Get().GetBrush("WhiteBrush"),
			ESlateDrawEffect::None,
			FMath::DegreesToRadians(MarkerRot),
			TOptional<FVector2D>(AbsCenter),
			FSlateDrawElement::ERotationSpace::RelativeToWorld,
			PlayerMarkerColor
		);
	}

	OutDrawElements.PopClip();

	DrawMinimapDecorations(OutDrawElements, CurrentLayer, AllottedGeometry);

	// ── 현재 목표 한 줄 ───────────────────────────────────────────────────────
	// 트래커를 못 보고 지나치는 플레이어를 위해 미니맵 바로 아래에도 목표를 붙인다.
	if (bShowObjectiveLine)
	{
		const UGameInstance* GI = GetGameInstance();
		const URetrieveGuidanceSubsystem* Guidance =
			GI ? GI->GetSubsystem<URetrieveGuidanceSubsystem>() : nullptr;

		if (Guidance && Guidance->HasObjective())
		{
			const FString Line = Guidance->GetObjectiveText().ToString();
			const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(
				"Bold", FMath::RoundToInt(ObjectiveLineFontSize * URetrieveUISettingsLibrary::GetUIScale()));

			const TSharedRef<FSlateFontMeasure> FontMeasure =
				FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			const FVector2D TextSize = FontMeasure->Measure(Line, Font);
			const FVector2D DrawPos(
				(WidgetSize.X - TextSize.X) * 0.5f,
				WidgetSize.Y - TextSize.Y - 2.0f);

			FSlateDrawElement::MakeText(
				OutDrawElements,
				++CurrentLayer,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(TextSize), FSlateLayoutTransform(FVector2f(DrawPos + FVector2D(1.0f)))),
				Line, Font, ESlateDrawEffect::None, FLinearColor(0.0f, 0.0f, 0.0f, 0.85f));

			FSlateDrawElement::MakeText(
				OutDrawElements,
				++CurrentLayer,
				AllottedGeometry.ToPaintGeometry(FVector2f(TextSize), FSlateLayoutTransform(FVector2f(DrawPos))),
				Line, Font, ESlateDrawEffect::None, ObjectiveLineColor);
		}
	}

	return CurrentLayer;
}

void URetrieveMinimapWidget::ToggleRotationMode()
{
	RotationMode = RotationMode == ERetrieveMinimapRotationMode::NorthUp
		? ERetrieveMinimapRotationMode::PlayerUp
		: ERetrieveMinimapRotationMode::NorthUp;
}

float URetrieveMinimapWidget::GetCameraYaw(const APlayerController* PC) const
{
	if (!PC)
	{
		return 0.0f;
	}
	return PC->PlayerCameraManager
		? PC->PlayerCameraManager->GetCameraRotation().Yaw
		: PC->GetControlRotation().Yaw;
}

FVector2D URetrieveMinimapWidget::Rotate2D(const FVector2D& V, float Degrees) const
{
	const float Rad = FMath::DegreesToRadians(Degrees);
	const float C   = FMath::Cos(Rad);
	const float S   = FMath::Sin(Rad);
	return FVector2D(V.X * C - V.Y * S, V.X * S + V.Y * C);
}

FVector2D URetrieveMinimapWidget::WorldToLocal(
	const FVector& TargetWorld,
	const FVector& PlayerWorld,
	const FVector2D& Center,
	const FVector2D& WidgetSize,
	float CameraYaw,
	float EffectiveViewWorldRadius
) const
{
	const float InvDiameter = 1.0f / FMath::Max(EffectiveViewWorldRadius * 2.0f, 1.0f);

	FVector2D ScreenDelta(
		 (TargetWorld.Y - PlayerWorld.Y) * WidgetSize.X * InvDiameter,
		-(TargetWorld.X - PlayerWorld.X) * WidgetSize.Y * InvDiameter
	);

	if (RotationMode == ERetrieveMinimapRotationMode::PlayerUp)
	{
		ScreenDelta = Rotate2D(ScreenDelta, -CameraYaw) * FMath::Sqrt(2.0f);
	}

	return Center + ScreenDelta;
}

void URetrieveMinimapWidget::DrawIcon(
	FSlateWindowElementList& OutDrawElements,
	int32& LayerId,
	const FGeometry& AllottedGeometry,
	const URetrieveMapIconComponent* Icon,
	const FVector2D& Pos
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

	if (Icon->IconType == ERetrieveMapIconType::Bonfire)
	{
		const ARetrieveBonfireActor* Bonfire = Cast<ARetrieveBonfireActor>(Icon->GetOwner());
		Color = (Bonfire && Bonfire->IsActivated()) ? BonfireActivatedColor : BonfireInactiveColor;
	}

	if (Icon->bIsDepleted)
	{
		Color *= DepletedIconTint;
	}

	const FVector2D IconSz(Size, Size);
	const FVector2D DrawPos = Pos - IconSz * 0.5f;

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
