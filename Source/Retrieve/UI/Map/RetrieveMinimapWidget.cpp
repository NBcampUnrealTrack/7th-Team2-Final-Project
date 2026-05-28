#include "UI/Map/RetrieveMinimapWidget.h"
#include "Subsystems/RetrieveMapSubsystem.h"
#include "Components/RetrieveMapIconComponent.h"
#include "Data/RetrieveMapIconRegistry.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "GameFramework/Pawn.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rendering/DrawElements.h"
#include "SlateMaterialBrush.h"
#include "Styling/CoreStyle.h"

void URetrieveMinimapWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	HideSquareMinimapBackground();
}

void URetrieveMinimapWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!Image_Minimap)
	{
		return;
	}

	HideSquareMinimapBackground();

	UMaterialInterface* BaseMaterial = MinimapMaterial;
	if (!BaseMaterial)
	{
		BaseMaterial = Cast<UMaterialInterface>(Image_Minimap->GetBrush().GetResourceObject());
	}

	if (!BaseMaterial)
	{
		return;
	}

	MinimapMID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	if (MinimapMID)
	{
		Image_Minimap->SetBrushFromMaterial(MinimapMID);

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
	if (!MapSub || !MapSub->HasValidBounds())
	{
		return;
	}

	const FVector PlayerLocation = PC->GetPawn()->GetActorLocation();
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
	
	if (ActiveTexture && ActiveTexture != CachedMIDTexture)
	{
		MinimapMID->SetTextureParameterValue(TEXT("MapTexture"), ActiveTexture);
		CachedMIDTexture = ActiveTexture;
	}
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

	DrawCircularMap(OutDrawElements, CurrentLayer, AllottedGeometry, CameraYaw);

	CurrentLayer = Super::NativePaint(
		Args, AllottedGeometry, MyCullingRect, OutDrawElements,
		CurrentLayer, InWidgetStyle, bParentEnabled
	);

	const FVector2D AbsCenter = FVector2D(AllottedGeometry.LocalToAbsolute(FVector2f(Center)));

	const FSlateClippingZone ClipZone(AllottedGeometry.GetLayoutBoundingRect());
	OutDrawElements.PushClip(ClipZone);
	
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
	
	if (World && MapSub && MapSub->HasValidBounds())
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

	OutDrawElements.PopClip();

	DrawMinimapDecorations(OutDrawElements, CurrentLayer, AllottedGeometry);

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
	float CameraYaw
) const
{
	const float InvDiameter = 1.0f / FMath::Max(ViewWorldRadius * 2.0f, 1.0f);

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
