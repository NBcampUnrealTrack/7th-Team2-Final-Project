#include "Subsystems/RetrieveMapSubsystem.h"
#include "Components/RetrieveMapIconComponent.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "Landscape.h"

void URetrieveMapSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	InitializeBounds(InWorld);
}

void URetrieveMapSubsystem::InitializeBounds(UWorld& InWorld)
{
	ASceneCapture2D* BestCapture = nullptr;
	int32 BestScore = MIN_int32;

	for (TActorIterator<ASceneCapture2D> It(&InWorld); It; ++It)
	{
		ASceneCapture2D* Capture = *It;
		if (!Capture) { continue; }

		const USceneCaptureComponent2D* Comp = Capture->GetCaptureComponent2D();
		if (!Comp || Comp->ProjectionType != ECameraProjectionMode::Orthographic) { continue; }

		int32 Score = FMath::RoundToInt(FMath::Max(Comp->OrthoWidth, 1.0f));
		const FString CaptureName = Capture->GetName();
		const FString TargetName = GetNameSafe(Comp->TextureTarget.Get());
		if (CaptureName.Contains(TEXT("Map")) || CaptureName.Contains(TEXT("Minimap")) || CaptureName.Contains(TEXT("WorldMap")))
		{
			Score += 1000000;
		}
		if (TargetName.Contains(TEXT("Map")) || TargetName.Contains(TEXT("Minimap")) || TargetName.Contains(TEXT("WorldMap")) || TargetName.Contains(TEXT("RT_Map")))
		{
			Score += 2000000;
		}

		if (!BestCapture || Score > BestScore)
		{
			BestCapture = Capture;
			BestScore = Score;
		}
	}

	if (BestCapture)
	{
		USceneCaptureComponent2D* Comp = BestCapture->GetCaptureComponent2D();
		const float W = FMath::Max(Comp ? Comp->OrthoWidth : 1.0f, 1.0f);
		const FVector Loc = BestCapture->GetActorLocation();

		MapOrigin = FVector2D(Loc.X - W * 0.5f, Loc.Y - W * 0.5f);
		MapExtentXY = FVector2D(W, W);
		MapExtent = W;

		UE_LOG(LogTemp, Log,
			TEXT("[Retrieve|Minimap] SceneCapture2D bounds: Capture=%s Origin=(%.0f,%.0f) Extent=%.0f"),
			*GetNameSafe(BestCapture), MapOrigin.X, MapOrigin.Y, MapExtent);

		// 베이크된 맵 텍스처가 할당돼 있으면 SceneCapture2D의 매 프레임 캡처를 끈다.
		// 범위 감지 이후 RT를 계속 렌더링할 이유가 없고, 끄지 않으면 렌더 타깃이
		// 매 프레임 GPU에 기록돼 텍스처 스트리밍 풀을 불필요하게 압박한다.
		if (BakedMapTexture && Comp && Comp->bCaptureEveryFrame)
		{
			Comp->bCaptureEveryFrame = false;
			UE_LOG(LogTemp, Log,
				TEXT("[Retrieve|Minimap] SceneCapture2D '%s' 매 프레임 캡처 비활성화 — 베이크 텍스처 모드"),
				*GetNameSafe(BestCapture));
		}
		return;
	}

	for (TActorIterator<ALandscape> It(&InWorld); It; ++It)
	{
		const ALandscape* Landscape = *It;
		if (!Landscape) { continue; }

		const FBox Bounds = Landscape->GetComponentsBoundingBox(true);

		const float Width = FMath::Max(Bounds.Max.X - Bounds.Min.X, 1.0f);
		const float Height = FMath::Max(Bounds.Max.Y - Bounds.Min.Y, 1.0f);

		MapExtentXY = FVector2D(Width, Height);
		MapExtent = FMath::Max(Width, Height);
		MapOrigin = FVector2D(Bounds.Min.X, Bounds.Min.Y);

		UE_LOG(LogTemp, Log,
			TEXT("[Retrieve|Minimap] Landscape bounds: Origin=(%.0f,%.0f) ExtXY=(%.0f x %.0f)"),
			MapOrigin.X, MapOrigin.Y, Width, Height);
		return;
	}
}

void URetrieveMapSubsystem::SetBakedMapTexture(UTexture2D* InTexture)
{
	BakedMapTexture = InTexture;
}

void URetrieveMapSubsystem::SetMapBounds(FVector2D InOrigin, float InExtent)
{
	MapOrigin = InOrigin;
	MapExtent = FMath::Max(InExtent, 1.0f);
	MapExtentXY = FVector2D(MapExtent, MapExtent);
}

void URetrieveMapSubsystem::SetMapBoundsXY(FVector2D InOrigin, float InExtentX, float InExtentY)
{
	MapOrigin = InOrigin;
	MapExtentXY = FVector2D(FMath::Max(InExtentX, 1.0f), FMath::Max(InExtentY, 1.0f));
	MapExtent = FMath::Max(MapExtentXY.X, MapExtentXY.Y);
}

FVector2D URetrieveMapSubsystem::WorldToUV(const FVector& WorldLocation) const
{
	const float SafeExtX = FMath::Max(MapExtentXY.X, 1.0f);
	const float SafeExtY = FMath::Max(MapExtentXY.Y, 1.0f);
	const float U = (WorldLocation.Y - MapOrigin.Y) / SafeExtY;
	const float V = 1.0f - (WorldLocation.X - MapOrigin.X) / SafeExtX;
	return FVector2D(U, V);
}

float URetrieveMapSubsystem::GetZoom(float ViewWorldRadius) const
{
	const float SafeRadius = FMath::Max(ViewWorldRadius, 1.0f);
	const float ShortExtent = FMath::Min(
		FMath::Max(MapExtentXY.X, 1.0f),
		FMath::Max(MapExtentXY.Y, 1.0f)
	);
	return ShortExtent / (SafeRadius * 2.0f);
}

void URetrieveMapSubsystem::DebugPrintBounds() const
{
	UE_LOG(LogTemp, Warning,
		TEXT("[Retrieve|Minimap] MapOrigin=(%.1f, %.1f) ExtentXY=(%.1f x %.1f) MapExtent=%.1f"),
		MapOrigin.X, MapOrigin.Y,
		MapExtentXY.X, MapExtentXY.Y,
		MapExtent
	);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1, 8.0f, FColor::Yellow,
			FString::Printf(
				TEXT("[Retrieve|Minimap] Origin=(%.0f,%.0f) ExtXY=(%.0f x %.0f) Extent=%.0f"),
				MapOrigin.X, MapOrigin.Y,
				MapExtentXY.X, MapExtentXY.Y,
				MapExtent
			)
		);
	}
}

UTexture2D* URetrieveMapSubsystem::GetTextureForUV(const FVector2D& UV) const
{
	for (const FRetrieveMapTile& Tile : MapTiles)
	{
		if (Tile.HasTexture() &&
		    UV.X >= Tile.UVMin.X && UV.X <= Tile.UVMax.X &&
		    UV.Y >= Tile.UVMin.Y && UV.Y <= Tile.UVMax.Y)
		{
			return Tile.Texture;
		}
	}

	return BakedMapTexture;
}

void URetrieveMapSubsystem::RegisterIcon(URetrieveMapIconComponent* Icon)
{
	if (IsValid(Icon) && IsValid(Icon->GetOwner()))
	{
		Icons.AddUnique(Icon);
	}
}

void URetrieveMapSubsystem::UnregisterIcon(URetrieveMapIconComponent* Icon)
{
	Icons.Remove(Icon);
}
