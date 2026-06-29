#include "World/RetrieveMapCaptureActor.h"

#include "Camera/CameraTypes.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
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

void ARetrieveMapCaptureActor::ConfigureCaptureForCleanMap()
{
	if (!SceneCapture)
	{
		return;
	}

	// 톤매핑된 LDR 결과를 그대로 텍스처로 사용.
	SceneCapture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;

	// 어둡고 흐릿하게 만드는 대기/안개/구름/후처리 제거.
	FEngineShowFlags& Flags = SceneCapture->ShowFlags;
	Flags.SetAtmosphere(false);
	Flags.SetFog(false);
	Flags.SetVolumetricFog(false);
	Flags.SetCloud(false);
	Flags.SetBloom(false);
	Flags.SetEyeAdaptation(false);          // 자동 노출로 어두워지는 것 방지
	Flags.SetMotionBlur(false);
	Flags.SetDepthOfField(false);
	Flags.SetLensFlares(false);
	Flags.SetVignette(false);
	Flags.SetSceneColorFringe(false);
	Flags.SetScreenSpaceReflections(false);
	Flags.SetTemporalAA(false);             // 단일 캡처에서 TAA 번짐 방지 → 더 선명

	// 언릿: 하늘/시간대/그림자 영향 제거(평면 베이스컬러). false면 라이팅 유지.
	Flags.SetLighting(!bCaptureUnlit);

	// 노출 고정 + 밝기 보정 (자동 노출 적응 비활성화).
	FPostProcessSettings& PP = SceneCapture->PostProcessSettings;
	PP.bOverride_AutoExposureMinBrightness = true;
	PP.AutoExposureMinBrightness = 1.0f;
	PP.bOverride_AutoExposureMaxBrightness = true;
	PP.AutoExposureMaxBrightness = 1.0f;
	PP.bOverride_AutoExposureBias = true;
	PP.AutoExposureBias = CaptureExposureBias;
	PP.bOverride_BloomIntensity = true;
	PP.BloomIntensity = 0.0f;
	PP.bOverride_VignetteIntensity = true;
	PP.VignetteIntensity = 0.0f;
	SceneCapture->PostProcessBlendWeight = 1.0f;
}

void ARetrieveMapCaptureActor::CaptureSceneWithCleanLighting()
{
	if (!SceneCapture)
	{
		return;
	}

	UWorld* World = GetWorld();

	// 언릿이거나 월드가 없으면 광원 손대지 않고 그대로 캡처.
	if (bCaptureUnlit || !World)
	{
		SceneCapture->CaptureScene();
		return;
	}

	// 캡처 동안만 복원할 광원 상태 저장용 (함수-로컬 구조체).
	struct FSavedLight
	{
		TWeakObjectPtr<UDirectionalLightComponent> Dir;
		TWeakObjectPtr<USkyLightComponent> Sky;
		FRotator Rotation = FRotator::ZeroRotator;
		float Intensity = 0.0f;
		FLinearColor Color = FLinearColor::White;
		bool bVisible = true;
	};
	TArray<FSavedLight> Saved;

	bool bMainSunAssigned = false;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor))
		{
			continue;
		}

		// ── 디렉셔널 라이트: 첫 번째를 고정 주간 태양으로, 나머지는 잠시 끔 ──
		TArray<UDirectionalLightComponent*> DirLights;
		Actor->GetComponents(DirLights);
		for (UDirectionalLightComponent* Dir : DirLights)
		{
			if (!Dir)
			{
				continue;
			}

			FSavedLight S;
			S.Dir       = Dir;
			S.Rotation  = Dir->GetComponentRotation();
			S.Intensity = Dir->Intensity;
			S.Color     = Dir->GetLightColor();
			S.bVisible  = Dir->IsVisible();
			Saved.Add(S);

			if (!bMainSunAssigned)
			{
				Dir->SetVisibility(true);
				Dir->SetWorldRotation(CaptureSunRotation);
				Dir->SetIntensity(CaptureSunIntensity);
				Dir->SetLightColor(CaptureSunColor);
				bMainSunAssigned = true;
			}
			else
			{
				Dir->SetVisibility(false);   // 달 등 보조 광원은 캡처 중 제외
			}
		}

		// ── 스카이라이트: 그림자부 채움용으로 세기만 고정 ──
		TArray<USkyLightComponent*> SkyLights;
		Actor->GetComponents(SkyLights);
		for (USkyLightComponent* Sky : SkyLights)
		{
			if (!Sky)
			{
				continue;
			}

			FSavedLight S;
			S.Sky       = Sky;
			S.Intensity = Sky->Intensity;
			S.bVisible  = Sky->IsVisible();
			Saved.Add(S);

			Sky->SetVisibility(true);
			Sky->Intensity = CaptureSkyLightIntensity;
			Sky->MarkRenderStateDirty();
		}
	}

	// 동기 캡처 (이 사이에 다이나믹 스카이 tick이 끼어들지 않음).
	SceneCapture->CaptureScene();

	// 원래 광원 상태 복원.
	for (const FSavedLight& S : Saved)
	{
		if (UDirectionalLightComponent* Dir = S.Dir.Get())
		{
			Dir->SetWorldRotation(S.Rotation);
			Dir->SetIntensity(S.Intensity);
			Dir->SetLightColor(S.Color);
			Dir->SetVisibility(S.bVisible);
		}
		else if (USkyLightComponent* Sky = S.Sky.Get())
		{
			Sky->Intensity = S.Intensity;
			Sky->SetVisibility(S.bVisible);
			Sky->MarkRenderStateDirty();
		}
	}
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

	// 선명한 맵을 위해 캡처 직전 하늘/안개/노출/후처리 정리.
	ConfigureCaptureForCleanMap();

	// 라이팅 모드면 캡처 동안만 고정 주간 광원을 강제 후 복원.
	CaptureSceneWithCleanLighting();

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