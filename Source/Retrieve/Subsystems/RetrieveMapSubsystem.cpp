#include "Subsystems/RetrieveMapSubsystem.h"
#include "Components/RetrieveMapIconComponent.h"

#include "EngineUtils.h"
#include "Landscape.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"

void URetrieveMapSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	// BeginPlay 이후 시점이므로 Landscape가 월드에 등록된 상태
	InitializeBounds(InWorld);
}

void URetrieveMapSubsystem::InitializeBounds(UWorld& InWorld)
{
	// ── 1순위: 직교 투영 SceneCapture2D에서 캡처 범위 자동 읽기 ─────────────
	// 탑뷰 베이크 텍스처는 SceneCapture 위치 + OrthoWidth로 정의되므로
	// Landscape 바운딩 박스보다 이 값이 더 정확하게 텍스처 UV와 일치한다.
	for (TActorIterator<ASceneCapture2D> It(&InWorld); It; ++It)
	{
		const ASceneCapture2D* Capture = *It;
		if (!Capture) { continue; }

		const USceneCaptureComponent2D* Comp = Capture->GetCaptureComponent2D();
		if (!Comp) { continue; }

		// 직교 투영이 아닌 SceneCapture (반사 캡처 등)는 건너뜀
		if (Comp->ProjectionType != ECameraProjectionMode::Orthographic) { continue; }

		const float     W   = FMath::Max(Comp->OrthoWidth, 1.0f);
		const FVector   Loc = Capture->GetActorLocation();

		// SceneCapture 중심 ± W/2 → 캡처 영역의 월드 Min 좌표
		MapOrigin   = FVector2D(Loc.X - W * 0.5f, Loc.Y - W * 0.5f);
		MapExtentXY = FVector2D(W, W);   // OrthoWidth는 정사각 캡처
		MapExtent   = W;

		UE_LOG(LogTemp, Log,
			TEXT("[Retrieve|Minimap] SceneCapture2D 자동 감지: Origin=(%.0f,%.0f)  Extent=%.0f"),
			MapOrigin.X, MapOrigin.Y, MapExtent);
		return;
	}

	// ── 2순위: Landscape 바운딩 박스 폴백 ────────────────────────────────────
	for (TActorIterator<ALandscape> It(&InWorld); It; ++It)
	{
		const ALandscape* Landscape = *It;
		if (!Landscape) { continue; }

		const FBox Bounds = Landscape->GetComponentsBoundingBox(true);

		const float Width  = FMath::Max(Bounds.Max.X - Bounds.Min.X, 1.0f);
		const float Height = FMath::Max(Bounds.Max.Y - Bounds.Min.Y, 1.0f);

		MapExtentXY = FVector2D(Width, Height);
		MapExtent   = FMath::Max(Width, Height);
		MapOrigin   = FVector2D(Bounds.Min.X, Bounds.Min.Y);

		UE_LOG(LogTemp, Log,
			TEXT("[Retrieve|Minimap] Landscape 바운드 감지: Origin=(%.0f,%.0f)  ExtXY=(%.0f x %.0f)"),
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
	MapOrigin   = InOrigin;
	MapExtent   = FMath::Max(InExtent, 1.0f);
	MapExtentXY = FVector2D(MapExtent, MapExtent);  // 정사각 레벨 가정
}

void URetrieveMapSubsystem::SetMapBoundsXY(FVector2D InOrigin, float InExtentX, float InExtentY)
{
	MapOrigin   = InOrigin;
	MapExtentXY = FVector2D(FMath::Max(InExtentX, 1.0f), FMath::Max(InExtentY, 1.0f));
	MapExtent   = FMath::Max(MapExtentXY.X, MapExtentXY.Y);  // 긴 변 = 미니맵 줌 기준
}

FVector2D URetrieveMapSubsystem::WorldToUV(const FVector& WorldLocation) const
{
	// 축별 실제 범위로 나눠 직사각형 레벨에서도 U/V 모두 [0,1] 전체 사용.
	// SceneCapture2D 탑뷰 베이크 텍스처 UV와 1:1 대응.
	//   U = East-West  (WorldY 축)
	//   V = North-South (WorldX 축, 위쪽=North=V0)
	const float SafeExtX = FMath::Max(MapExtentXY.X, 1.0f);  // V 방향 (North-South)
	const float SafeExtY = FMath::Max(MapExtentXY.Y, 1.0f);  // U 방향 (East-West)
	const float U =        (WorldLocation.Y - MapOrigin.Y) / SafeExtY;
	const float V = 1.0f - (WorldLocation.X - MapOrigin.X) / SafeExtX;
	return FVector2D(U, V);
}

float URetrieveMapSubsystem::GetZoom(float ViewWorldRadius) const
{
	// 짧은 축 기준: 짧은 방향이 정확히 ViewWorldRadius를 채움.
	// 긴 축은 약간 더 넓게 보일 수 있으나, 아이콘이 잘리지 않음.
	const float SafeRadius  = FMath::Max(ViewWorldRadius, 1.0f);
	const float ShortExtent = FMath::Min(
		FMath::Max(MapExtentXY.X, 1.0f),
		FMath::Max(MapExtentXY.Y, 1.0f)
	);
	return ShortExtent / (SafeRadius * 2.0f);
}

void URetrieveMapSubsystem::DebugPrintBounds() const
{
	UE_LOG(LogTemp, Warning,
		TEXT("[Retrieve|Minimap] MapOrigin=(%.1f, %.1f)  ExtentXY=(%.1f x %.1f)  MapExtent=%.1f"),
		MapOrigin.X, MapOrigin.Y,
		MapExtentXY.X, MapExtentXY.Y,
		MapExtent
	);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1, 8.0f, FColor::Yellow,
			FString::Printf(
				TEXT("[Retrieve|Minimap] Origin=(%.0f,%.0f)  ExtXY=(%.0f x %.0f)  Extent=%.0f"),
				MapOrigin.X, MapOrigin.Y,
				MapExtentXY.X, MapExtentXY.Y,
				MapExtent
			)
		);
	}
}

UTexture2D* URetrieveMapSubsystem::GetTextureForUV(const FVector2D& UV) const
{
	// 타일 모드: UV를 포함하는 타일 텍스처 반환
	for (const FRetrieveMapTile& Tile : MapTiles)
	{
		if (Tile.HasTexture() &&
		    UV.X >= Tile.UVMin.X && UV.X <= Tile.UVMax.X &&
		    UV.Y >= Tile.UVMin.Y && UV.Y <= Tile.UVMax.Y)
		{
			return Tile.Texture;
		}
	}
	// 단일 텍스처 폴백
	return BakedMapTexture;
}

void URetrieveMapSubsystem::RegisterIcon(URetrieveMapIconComponent* Icon)
{
	if (Icon)
	{
		Icons.AddUnique(Icon);
	}
}

void URetrieveMapSubsystem::UnregisterIcon(URetrieveMapIconComponent* Icon)
{
	Icons.Remove(Icon);
}
