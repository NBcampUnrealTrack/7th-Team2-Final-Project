#include "UI/Map/RetrieveMinimapWidget.h"
#include "Subsystems/RetrieveMapSubsystem.h"
#include "Components/RetrieveMapIconComponent.h"
#include "Data/RetrieveMapIconRegistry.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

// ---------- 초기화 ----------

void URetrieveMinimapWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!Image_Minimap)
	{
		return;
	}

	UMaterialInterface* BaseMaterial = MinimapMaterial;
	if (!BaseMaterial)
	{
		BaseMaterial = Cast<UMaterialInterface>(Image_Minimap->GetBrush().GetResourceObject());
	}

	// The image widget is only a design-time/material reference. NativePaint owns
	// the minimap render order so the brush cannot cover markers or actor icons.
	Image_Minimap->SetVisibility(ESlateVisibility::Collapsed);

	if (!BaseMaterial)
	{
		return;
	}

	MinimapMID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	if (MinimapMID)
	{
		// 맵 텍스처는 NativePaint에서 직접 그리므로 Image_Minimap 위젯은 숨긴다.
		// (MID 생성을 위한 머티리얼 참조 용도로만 사용)
		Image_Minimap->SetVisibility(ESlateVisibility::Collapsed);

		// 머티리얼 기본값이 RT 텍스처를 참조할 수 있으므로 베이크 텍스처를 즉시 덮어씌운다.
		// NativeTick 전에 설정해 첫 프레임부터 올바른 텍스처가 사용되도록 한다.
		UTexture2D* InitTex = BakedMapTexture;
		if (!InitTex)
		{
			if (UWorld* W = GetWorld())
			{
				if (URetrieveMapSubsystem* Sub = W->GetSubsystem<URetrieveMapSubsystem>())
				{
					InitTex = Sub->BakedMapTexture;
				}
			}
		}
		if (InitTex)
		{
			MinimapMID->SetTextureParameterValue(TEXT("MapTexture"), InitTex);
			CachedMIDTexture = InitTex;
		}
	}
}

// ---------- 매 프레임 ----------

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
	if (!MapSub || !MapSub->HasValidBounds())
	{
		return;
	}

	const FVector PlayerLocation = PC->GetPawn()->GetActorLocation();
	if (MinimapMID)
	{
		UpdateMinimapMaterial(MapSub, PlayerLocation);
	}
}

// ---------- 머티리얼 파라미터 갱신 ----------

void URetrieveMinimapWidget::UpdateMinimapMaterial(
	URetrieveMapSubsystem* MapSub,
	const FVector& PlayerLocation
)
{
	const FVector2D PlayerUV = MapSub->WorldToUV(PlayerLocation);
	const float Zoom = MapSub->GetZoom(ViewWorldRadius);

	MinimapMID->SetVectorParameterValue(
		TEXT("CenterUV"),
		FLinearColor(PlayerUV.X, PlayerUV.Y, 0.0f, 0.0f)
	);
	MinimapMID->SetScalarParameterValue(TEXT("Zoom"), Zoom);

	UTexture2D* ActiveTexture = BakedMapTexture;
	if (!ActiveTexture && MapSub->BakedMapTexture)
	{
		ActiveTexture = MapSub->BakedMapTexture;
	}

	// 텍스처가 실제로 바뀔 때만 파라미터를 업데이트한다.
	// 매 틱 SetTextureParameterValue를 호출하면 스트리밍 시스템이 해당 텍스처를
	// 최고 우선순위로 유지하려 하므로 스트리밍 풀 압박을 가중시킨다.
	if (ActiveTexture && ActiveTexture != CachedMIDTexture)
	{
		MinimapMID->SetTextureParameterValue(TEXT("MapTexture"), ActiveTexture);
		CachedMIDTexture = ActiveTexture;
	}
}

// ---------- 페인트 (맵 텍스처 + 마커 + 아이콘) ----------

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
	int32 CurrentLayer = Super::NativePaint(
		Args, AllottedGeometry, MyCullingRect, OutDrawElements,
		LayerId, InWidgetStyle, bParentEnabled
	);

	const FVector2D WidgetSize = AllottedGeometry.GetLocalSize();
	if (WidgetSize.X <= 1.0f || WidgetSize.Y <= 1.0f)
	{
		return CurrentLayer;
	}

	const APlayerController* PC = GetOwningPlayer();
	if (!PC || !PC->GetPawn())
	{
		return CurrentLayer;
	}

	const FVector2D Center        = WidgetSize * 0.5f;
	const float     MiniMapRadius = FMath::Min(WidgetSize.X, WidgetSize.Y) * 0.5f;
	const FVector   PlayerLoc     = PC->GetPawn()->GetActorLocation();
	const float     CameraYaw     = GetCameraYaw(PC);
	UWorld* World = GetWorld();
	URetrieveMapSubsystem* MapSub = World ? World->GetSubsystem<URetrieveMapSubsystem>() : nullptr;

	// 모든 드로우 콜을 위젯 할당 영역으로 클리핑 (맵 텍스처 오버플로우 방지)
	const FSlateClippingZone ClipZone(AllottedGeometry.GetLayoutBoundingRect());
	OutDrawElements.PushClip(ClipZone);

	// RelativeToWorld 회전 기준점: 위젯 중앙의 절대 화면 좌표
	const FVector2D AbsCenter = FVector2D(AllottedGeometry.LocalToAbsolute(FVector2f(Center)));

	// ── 맵 텍스처 ─────────────────────────────────────────────────────────────
	// PlayerUp: -CameraYaw로 CCW 회전 → 플레이어 진행 방향이 위쪽에 고정
	// 회전 시 모서리 공백이 생기지 않도록 √2 배 확대한 뒤 클리핑
	if (MinimapMID)
	{
		const float RotAngle = (RotationMode == ERetrieveMinimapRotationMode::PlayerUp) ? -CameraYaw : 0.0f;
		// PlayerUp: 항상 √2 배 고정 확대 → 회전 각도와 무관하게 모서리를 채우면서 줌 변동 없음
		const float Scale    = (RotationMode == ERetrieveMinimapRotationMode::PlayerUp) ? FMath::Sqrt(2.0f) : 1.0f;

		const FVector2D ScaledSize = WidgetSize * Scale;
		const FVector2D MapTopLeft = Center - ScaledSize * 0.5f;

		FSlateBrush MapBrush;
		MapBrush.SetResourceObject(MinimapMID);
		MapBrush.ImageSize = WidgetSize;

		FSlateDrawElement::MakeRotatedBox(
			OutDrawElements,
			++CurrentLayer,
			AllottedGeometry.ToPaintGeometry(
				FVector2f(ScaledSize),
				FSlateLayoutTransform(FVector2f(MapTopLeft))
			),
			&MapBrush,
			ESlateDrawEffect::None,
			FMath::DegreesToRadians(RotAngle),
			TOptional<FVector2D>(AbsCenter),
			FSlateDrawElement::ERotationSpace::RelativeToWorld
		);
	}
	else if (MapSub && MapSub->HasValidBounds())
	{
		UTexture2D* ActiveTexture = BakedMapTexture ? BakedMapTexture.Get() : MapSub->BakedMapTexture.Get();
		if (ActiveTexture)
		{
			const FVector2D PlayerUV = MapSub->WorldToUV(PlayerLoc);
			const float HalfU = ViewWorldRadius / FMath::Max(MapSub->MapExtentXY.Y, 1.0f);
			const float HalfV = ViewWorldRadius / FMath::Max(MapSub->MapExtentXY.X, 1.0f);
			const FVector2D UVMin(
				FMath::Clamp(PlayerUV.X - HalfU, 0.0f, 1.0f),
				FMath::Clamp(PlayerUV.Y - HalfV, 0.0f, 1.0f)
			);
			const FVector2D UVMax(
				FMath::Clamp(PlayerUV.X + HalfU, 0.0f, 1.0f),
				FMath::Clamp(PlayerUV.Y + HalfV, 0.0f, 1.0f)
			);

			const float RotAngle = (RotationMode == ERetrieveMinimapRotationMode::PlayerUp) ? -CameraYaw : 0.0f;
			const float Scale = (RotationMode == ERetrieveMinimapRotationMode::PlayerUp) ? FMath::Sqrt(2.0f) : 1.0f;
			const FVector2D ScaledSize = WidgetSize * Scale;
			const FVector2D MapTopLeft = Center - ScaledSize * 0.5f;

			FSlateBrush MapBrush;
			MapBrush.SetResourceObject(ActiveTexture);
			MapBrush.ImageSize = WidgetSize;
			MapBrush.SetUVRegion(FBox2f(FVector2f(UVMin), FVector2f(UVMax)));

			FSlateDrawElement::MakeRotatedBox(
				OutDrawElements,
				++CurrentLayer,
				AllottedGeometry.ToPaintGeometry(
					FVector2f(ScaledSize),
					FSlateLayoutTransform(FVector2f(MapTopLeft))
				),
				&MapBrush,
				ESlateDrawEffect::None,
				FMath::DegreesToRadians(RotAngle),
				TOptional<FVector2D>(AbsCenter),
				FSlateDrawElement::ERotationSpace::RelativeToWorld
			);
		}
	}

	// ── 플레이어 마커 ──────────────────────────────────────────────────────────
	// NorthUp: CameraYaw만큼 회전해 북쪽 기준 방향 표시
	// PlayerUp: 마커 고정 (위쪽 = 진행 방향)
	{
		const float MarkerRot = (RotationMode == ERetrieveMinimapRotationMode::NorthUp) ? CameraYaw : 0.0f;

		const FVector2D MarkerSz(PlayerMarkerSize, PlayerMarkerSize);
		const FVector2D MarkerPos = Center - MarkerSz * 0.5f;

		FSlateBrush Brush;
		Brush.SetResourceObject(PlayerMarkerTexture);
		Brush.ImageSize = MarkerSz;

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

	// ── 액터 아이콘 ──────────────────────────────────────────────────────────
	if (World)
	{
		if (MapSub && MapSub->HasValidBounds())
		{
			for (const URetrieveMapIconComponent* Icon : MapSub->GetIcons())
			{
				if (!IsValid(Icon) || !IsValid(Icon->GetOwner()) || !Icon->bShowOnMinimap)
				{
					continue;
				}

				const FVector IconWorld = Icon->GetOwner()->GetActorLocation();
				if (FVector::Dist2D(PlayerLoc, IconWorld) > ViewWorldRadius)
				{
					continue;
				}

				const FVector2D IconPos = WorldToLocal(
					IconWorld, PlayerLoc, Center, WidgetSize, CameraYaw
				);

				if (FVector2D::Distance(IconPos, Center) > MiniMapRadius)
				{
					continue;
				}

				DrawIcon(OutDrawElements, CurrentLayer, AllottedGeometry, Icon, IconPos);
			}
		}
	}

	OutDrawElements.PopClip();

	return CurrentLayer;
}

// ---------- 헬퍼 ----------

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
	float CameraYaw
) const
{
	// 월드 공간 직접 델타 계산 — 레벨 형태(직사각/정사각)에 무관하게 정확.
	//   +Y(East)  → 화면 오른쪽 (+ScreenX)
	//   +X(North) → 화면 위쪽   (-ScreenY, 화면 Y 축 반전)
	// ViewWorldRadius가 위젯 반쪽(WidgetSize/2)에 대응.
	const float InvDiameter = 1.0f / FMath::Max(ViewWorldRadius * 2.0f, 1.0f);

	FVector2D ScreenDelta(
		 (TargetWorld.Y - PlayerWorld.Y) * WidgetSize.X * InvDiameter,
		-(TargetWorld.X - PlayerWorld.X) * WidgetSize.Y * InvDiameter   // North-South 반전
	);

	if (RotationMode == ERetrieveMinimapRotationMode::PlayerUp)
	{
		// 맵 텍스처가 -CameraYaw로 그려지므로 아이콘도 동일 방향 회전.
		// 맵이 √2 고정 확대되므로 아이콘 오프셋도 √2 배 적용.
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
