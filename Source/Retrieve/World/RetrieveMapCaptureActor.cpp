#include "World/RetrieveMapCaptureActor.h"

#include "Camera/CameraTypes.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Data/RetrieveMapConfigDataAsset.h"
#include "Data/RetrieveMapIconDataAsset.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"

#if WITH_EDITOR
#include "Editor.h"
#endif

ARetrieveMapCaptureActor::ARetrieveMapCaptureActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCapture"));
	SetRootComponent(SceneCapture);

	SceneCapture->ProjectionType = ECameraProjectionMode::Orthographic;
	SceneCapture->bCaptureEveryFrame = false;
	SceneCapture->bCaptureOnMovement = false;
	SceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	SceneCapture->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
}

#if WITH_EDITOR

bool ARetrieveMapCaptureActor::CalculateTaggedBounds(FBox& OutBounds) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	OutBounds.Init();

	int32 Count = 0;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor) || Actor == this)
		{
			continue;
		}

		if (!Actor->ActorHasTag(BoundsIncludeTag))
		{
			continue;
		}

		const FBox ActorBounds = Actor->GetComponentsBoundingBox(true);
		if (!ActorBounds.IsValid)
		{
			continue;
		}

		OutBounds += ActorBounds;
		++Count;
	}

	if (Count <= 0 || !OutBounds.IsValid)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[MapCapture] BoundsIncludeTag='%s' 액터를 찾지 못했습니다."),
			*BoundsIncludeTag.ToString());
		return false;
	}

	OutBounds = OutBounds.ExpandBy(FVector(BoundsPadding, BoundsPadding, 0.0f));

	UE_LOG(LogTemp, Log,
		TEXT("[MapCapture] Bounds 계산 완료: Count=%d Min=(%.0f, %.0f) Max=(%.0f, %.0f)"),
		Count,
		OutBounds.Min.X, OutBounds.Min.Y,
		OutBounds.Max.X, OutBounds.Max.Y);

	return true;
}

void ARetrieveMapCaptureActor::UpdateMapConfig(
	UTexture2D* SavedTexture,
	const FVector2D& Origin,
	const FVector2D& ExtentXY)
{
	if (!MapConfig)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MapCapture] MapConfig가 없습니다."));
		return;
	}

	// 현재 단계에서는 Texture2D 자동 저장을 하지 않는다.
	// DA_MapConfig의 BakedMapTexture는 에디터에서 수동 할당한 값을 유지한다.
	if (SavedTexture)
	{
		MapConfig->BakedMapTexture = SavedTexture;
	}

	MapConfig->MapOrigin = Origin;
	MapConfig->MapExtentXY = ExtentXY;

	if (WorldMapIconData)
	{
		MapConfig->WorldMapIconData = WorldMapIconData;
	}

	MapConfig->MarkPackageDirty();

	UE_LOG(LogTemp, Log,
		TEXT("[MapCapture] MapConfig 갱신 완료: Origin=(%.0f, %.0f), ExtentXY=(%.0f x %.0f), Texture=%s"),
		Origin.X, Origin.Y,
		ExtentXY.X, ExtentXY.Y,
		*GetNameSafe(MapConfig->BakedMapTexture));
}

void ARetrieveMapCaptureActor::RefreshMapAll()
{
	if (!SceneCapture)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MapCapture] SceneCapture가 없습니다."));
		return;
	}

	if (!RenderTarget)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MapCapture] RenderTarget이 없습니다."));
		return;
	}

	FBox Bounds;
	if (!CalculateTaggedBounds(Bounds))
	{
		return;
	}

	const FVector Center = Bounds.GetCenter();
	const FVector Extent = Bounds.GetExtent();

	float CaptureWidthX = Extent.X * 2.0f;
	float CaptureWidthY = Extent.Y * 2.0f;

	if (bUseSquareCaptureBounds)
	{
		const float SquareSize = FMath::Max(CaptureWidthX, CaptureWidthY);
		CaptureWidthX = SquareSize;
		CaptureWidthY = SquareSize;
	}

	const float OrthoWidth = CaptureWidthY;

	const FVector CaptureLocation(Center.X, Center.Y, CaptureHeight);
	SetActorLocation(CaptureLocation);
	SetActorRotation(FRotator(-90.0f, 0.0f, 0.0f));

	SceneCapture->TextureTarget = RenderTarget;
	SceneCapture->ProjectionType = ECameraProjectionMode::Orthographic;
	SceneCapture->OrthoWidth = OrthoWidth;
	SceneCapture->CaptureScene();

	const FVector2D Origin(
		Center.X - CaptureWidthX * 0.5f,
		Center.Y - CaptureWidthY * 0.5f
	);

	const FVector2D ExtentXY(CaptureWidthX, CaptureWidthY);

	// UE5.7에서 RenderTarget → Texture2D 자동 저장 API가 달라져서
	// 우선 자동 에셋 생성은 제외하고, 기존 DA_MapConfig.BakedMapTexture는 유지한다.
	UpdateMapConfig(nullptr, Origin, ExtentXY);

	if (WorldMapIconData)
	{
		WorldMapIconData->RefreshFromLevel();
		WorldMapIconData->MarkPackageDirty();
	}

	UE_LOG(LogTemp, Log,
		TEXT("[MapCapture] RefreshMapAll 완료: OrthoWidth=%.0f Origin=(%.0f, %.0f) ExtentXY=(%.0f x %.0f)"),
		OrthoWidth,
		Origin.X, Origin.Y,
		ExtentXY.X, ExtentXY.Y);
}

#endif