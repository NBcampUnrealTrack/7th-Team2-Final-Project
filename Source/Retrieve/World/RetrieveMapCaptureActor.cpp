#include "World/RetrieveMapCaptureActor.h"

#include "Camera/CameraTypes.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/DecalComponent.h"
#include "Components/MeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "LandscapeSplineActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Data/RetrieveMapConfigDataAsset.h"
#include "Data/RetrieveMapIconDataAsset.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "RenderingThread.h"
#include "HAL/IConsoleManager.h"

#if WITH_EDITOR
#include "Editor.h"

namespace
{
	// 오소그래픽 전체 레벨 캡처 시 텍스처 스트리밍의 밉 선택 휴리스틱이 카메라 거리를 잘못
	// 계산해, 화면에 보이는 모든 텍스처에 최대 해상도 밉맵을 한꺼번에 요구해 VRAM 버짓 경고가
	// 폭주하는 경우가 있다. 캡처 동안만 스트리밍을 끄고, 스코프를 벗어나면(성공/실패/조기 리턴
	// 모두) 원래 값으로 복원한다.
	struct FScopedTextureStreamingDisable
	{
		IConsoleVariable* CVar = nullptr;
		int32 PrevValue = 1;

		FScopedTextureStreamingDisable()
		{
			CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.TextureStreaming"));
			if (CVar)
			{
				PrevValue = CVar->GetInt();
				CVar->Set(0, ECVF_SetByCode);
			}
		}

		~FScopedTextureStreamingDisable()
		{
			if (CVar)
			{
				CVar->Set(PrevValue, ECVF_SetByCode);
			}
		}
	};

	// SrcW/SrcH 크기의 픽셀 버퍼를 Factor만큼 박스필터(단순 평균)로 다운샘플링한다.
	void DownsamplePixelsBoxFilter(
		const TArray<FColor>& Src, int32 SrcW, int32 SrcH, int32 Factor,
		TArray<FColor>& OutDst, int32& OutDstW, int32& OutDstH)
	{
		OutDstW = SrcW / Factor;
		OutDstH = SrcH / Factor;
		OutDst.SetNumUninitialized(OutDstW * OutDstH);

		for (int32 Y = 0; Y < OutDstH; ++Y)
		{
			for (int32 X = 0; X < OutDstW; ++X)
			{
				uint32 R = 0, G = 0, B = 0, A = 0;
				for (int32 SY = 0; SY < Factor; ++SY)
				{
					const int32 RowBase = (Y * Factor + SY) * SrcW;
					for (int32 SX = 0; SX < Factor; ++SX)
					{
						const FColor& C = Src[RowBase + X * Factor + SX];
						R += C.R; G += C.G; B += C.B; A += C.A;
					}
				}
				const uint32 Count = static_cast<uint32>(Factor * Factor);
				// 알파는 반전 규약(지형=0, 빈공간=255)으로 쓰인다 — 강제 고정하지 말고 그대로 평균.
				// (RetrieveWorldMapWidget.h의 MapDisplayMaterial 주석 참고: Opacity = 1 - Tex.A)
				OutDst[Y * OutDstW + X] = FColor(
					static_cast<uint8>(R / Count), static_cast<uint8>(G / Count),
					static_cast<uint8>(B / Count), static_cast<uint8>(A / Count));
			}
		}
	}
}
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
	// (SCS_SceneColorHDR로 시험했으나 Single Layer Water 강 렌더링 문제가 그대로였고
	//  톤매핑 없는 부작용만 있어 원복 — 이 문제는 캡처 소스와 무관한 엔진 레벨 한계로 보임.)
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

	// BP_StylizedSky 같은 동적 하늘 액터는 라이트·스카이라이트·안개·포스트프로세스가 전부
	// 시간대에 따라 바뀌는데, 그중 일부만 골라 고정해도 나머지(스카이라이트 큐브맵/색 등)
	// 영향이 새어 들어와 캡처마다 색감이 달라진다. 액터 전체를 숨기면 물 액터처럼 언더워터
	// 포스트프로세스용으로 PostProcessComponent를 같이 들고 있는 다른 액터(메시 포함)까지
	// 통째로 사라지므로, 액터 단위가 아니라 "문제되는 컴포넌트"만 개별적으로 끈다.
	// 대신 완전히 독립적인 임시 라이트를 그 순간만 스폰해 100% 동일한 결과를 보장한다.
	struct FSavedComponentState
	{
		TWeakObjectPtr<USceneComponent> LightVisComp; // DirectionalLight/SkyLight: 비저빌리티로 끔
		TWeakObjectPtr<UPostProcessComponent> PP;     // PostProcess: bEnabled로 끔
		TWeakObjectPtr<UExponentialHeightFogComponent> Fog; // Fog: 비저빌리티로 끔
		bool bWasVisibleOrEnabled = true;
	};
	TArray<FSavedComponentState> SavedComponents;

	// 가늘고 긴 강줄기는 데칼(DecalComponent)로 지형에 투영되는 경우가 있는데, 데칼은
	// 화면에서 너무 작아 보이면 자동으로 페이드아웃(FadeScreenSize)한다. 레벨 전체를 아주
	// 먼 거리에서 캡처하면 이 페이드 규칙에 걸려 강이 아예 안 그려지므로, 캡처 동안만
	// 페이드를 꺼둔다(FadeScreenSize=0 → 항상 렌더링).
	struct FSavedDecalFade
	{
		TWeakObjectPtr<UDecalComponent> Decal;
		float FadeScreenSize = 0.0f;
	};
	TArray<FSavedDecalFade> SavedDecalFade;

	// LandscapeSplineActor(강줄기가 스플라인 메시로 배치되는 경우)는 Single Layer Water
	// 계열 머티리얼을 써서 SceneCaptureComponent2D에서 정상 렌더되지 않는다(라이트/포스트
	// 프로세스/데칼 조정으로 해결 안 됨을 확인). 이 액터 타입에 한해서만(다른 워터 액터는
	// 건드리지 않음) 메시 머티리얼을 캡처 동안 단순 단색으로 바꿔치기했다가 복원한다.
	static UMaterialInterface* const WaterFallbackMaterial = LoadObject<UMaterialInterface>(
		nullptr, TEXT("/Game/Retrieve/Data/Map/M_MapCaptureWaterFallback.M_MapCaptureWaterFallback"));

	struct FSavedMeshMaterial
	{
		TWeakObjectPtr<UMeshComponent> Mesh;
		int32 SlotIndex = 0;
		TWeakObjectPtr<UMaterialInterface> OriginalMaterial;
	};
	TArray<FSavedMeshMaterial> SavedMeshMaterials;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!IsValid(Actor) || Actor == this)
		{
			continue;
		}

		TArray<UDirectionalLightComponent*> DirLights;
		Actor->GetComponents(DirLights);
		for (UDirectionalLightComponent* Dir : DirLights)
		{
			if (!Dir) { continue; }
			FSavedComponentState S;
			S.LightVisComp = Dir;
			S.bWasVisibleOrEnabled = Dir->IsVisible();
			SavedComponents.Add(S);
			Dir->SetVisibility(false);
		}

		TArray<USkyLightComponent*> SkyLights;
		Actor->GetComponents(SkyLights);
		for (USkyLightComponent* Sky : SkyLights)
		{
			if (!Sky) { continue; }
			FSavedComponentState S;
			S.LightVisComp = Sky;
			S.bWasVisibleOrEnabled = Sky->IsVisible();
			SavedComponents.Add(S);
			Sky->SetVisibility(false);
		}

		TArray<UPostProcessComponent*> PostProcessComps;
		Actor->GetComponents(PostProcessComps);
		for (UPostProcessComponent* PP : PostProcessComps)
		{
			if (!PP) { continue; }
			FSavedComponentState S;
			S.PP = PP;
			S.bWasVisibleOrEnabled = PP->bEnabled;
			SavedComponents.Add(S);
			PP->bEnabled = false;
		}

		TArray<UExponentialHeightFogComponent*> FogComps;
		Actor->GetComponents(FogComps);
		for (UExponentialHeightFogComponent* Fog : FogComps)
		{
			if (!Fog) { continue; }
			FSavedComponentState S;
			S.Fog = Fog;
			S.bWasVisibleOrEnabled = Fog->IsVisible();
			SavedComponents.Add(S);
			Fog->SetVisibility(false);
		}

		TArray<UDecalComponent*> Decals;
		Actor->GetComponents(Decals);
		for (UDecalComponent* Decal : Decals)
		{
			if (!Decal) { continue; }
			FSavedDecalFade S;
			S.Decal = Decal;
			S.FadeScreenSize = Decal->FadeScreenSize;
			SavedDecalFade.Add(S);
			Decal->FadeScreenSize = 0.0f;
		}

		if (WaterFallbackMaterial && Actor->IsA<ALandscapeSplineActor>())
		{
			TArray<UMeshComponent*> MeshComps;
			Actor->GetComponents(MeshComps);
			for (UMeshComponent* Mesh : MeshComps)
			{
				if (!Mesh) { continue; }

				const int32 NumMats = Mesh->GetNumMaterials();
				for (int32 SlotIndex = 0; SlotIndex < NumMats; ++SlotIndex)
				{
					FSavedMeshMaterial S;
					S.Mesh = Mesh;
					S.SlotIndex = SlotIndex;
					S.OriginalMaterial = Mesh->GetMaterial(SlotIndex);
					SavedMeshMaterials.Add(S);

					Mesh->SetMaterial(SlotIndex, WaterFallbackMaterial);
				}
			}
		}
	}

	// 캡처 전용 임시 라이트 — 완전히 독립적, 항상 동일한 값.
	ADirectionalLight* TempSun = World->SpawnActor<ADirectionalLight>();
	ASkyLight* TempSkyLight = World->SpawnActor<ASkyLight>();

	if (TempSun)
	{
		TempSun->SetActorRotation(CaptureSunRotation);
		if (UDirectionalLightComponent* SunComp = Cast<UDirectionalLightComponent>(TempSun->GetLightComponent()))
		{
			SunComp->SetIntensity(CaptureSunIntensity);
			SunComp->SetLightColor(CaptureSunColor);
		}
	}

	if (TempSkyLight)
	{
		if (USkyLightComponent* SkyComp = Cast<USkyLightComponent>(TempSkyLight->GetLightComponent()))
		{
			SkyComp->SetIntensity(CaptureSkyLightIntensity);
			SkyComp->SetLightColor(FLinearColor::White);
			SkyComp->RecaptureSky();
		}
	}

	// 동기 캡처 (이 사이에 다이나믹 스카이 tick이 끼어들지 않음).
	SceneCapture->CaptureScene();

	// 임시 라이트 제거.
	if (TempSun)
	{
		TempSun->Destroy();
	}
	if (TempSkyLight)
	{
		TempSkyLight->Destroy();
	}

	// 꺼뒀던 컴포넌트 원복.
	for (const FSavedComponentState& S : SavedComponents)
	{
		if (UDirectionalLightComponent* Dir = Cast<UDirectionalLightComponent>(S.LightVisComp.Get()))
		{
			Dir->SetVisibility(S.bWasVisibleOrEnabled);
		}
		else if (USkyLightComponent* Sky = Cast<USkyLightComponent>(S.LightVisComp.Get()))
		{
			Sky->SetVisibility(S.bWasVisibleOrEnabled);
		}
		else if (UPostProcessComponent* PP = S.PP.Get())
		{
			PP->bEnabled = S.bWasVisibleOrEnabled;
		}
		else if (UExponentialHeightFogComponent* Fog = S.Fog.Get())
		{
			Fog->SetVisibility(S.bWasVisibleOrEnabled);
		}
	}

	// 꺼뒀던 데칼 페이드 원복.
	for (const FSavedDecalFade& S : SavedDecalFade)
	{
		if (UDecalComponent* Decal = S.Decal.Get())
		{
			Decal->FadeScreenSize = S.FadeScreenSize;
		}
	}

	// 바꿔치기했던 LandscapeSplineActor 워터 머티리얼 원복.
	for (const FSavedMeshMaterial& S : SavedMeshMaterials)
	{
		if (UMeshComponent* Mesh = S.Mesh.Get())
		{
			Mesh->SetMaterial(S.SlotIndex, S.OriginalMaterial.Get());
		}
	}
}

void ARetrieveMapCaptureActor::CaptureWithSupersampling()
{
	// 함수를 벗어나는 모든 경로(성공/실패/조기 리턴)에서 텍스처 스트리밍을 원래 상태로 복원.
	FScopedTextureStreamingDisable StreamingGuard;

	const int32 Factor = FMath::Clamp(SuperSampleFactor, 1, 8);
	const int32 FinalSizeX = RenderTarget->SizeX;
	const int32 FinalSizeY = RenderTarget->SizeY;

	// Factor > 1: 임시 고해상도 렌더타겟에 캡처(슈퍼샘플). Factor == 1: RenderTarget에 바로 캡처.
	UTextureRenderTarget2D* HighResTarget = nullptr;
	UTextureRenderTarget2D* CaptureTarget = RenderTarget;

	if (Factor > 1)
	{
		HighResTarget = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient);
		HighResTarget->InitAutoFormat(FinalSizeX * Factor, FinalSizeY * Factor);
		HighResTarget->UpdateResourceImmediate(true);
		CaptureTarget = HighResTarget;
	}

	SceneCapture->TextureTarget = CaptureTarget;
	ConfigureCaptureForCleanMap();
	CaptureSceneWithCleanLighting();

	// 원래 RenderTarget으로 복원 — 다른 코드/에디터가 계속 이 프로퍼티를 참조하므로.
	SceneCapture->TextureTarget = RenderTarget;

	FTextureRenderTargetResource* CaptureResource = CaptureTarget->GameThread_GetRenderTargetResource();
	TArray<FColor> CapturedPixels;
	const bool bReadOk = CaptureResource && CaptureResource->ReadPixels(CapturedPixels);

	if (HighResTarget)
	{
		// 픽셀을 다 읽었으면 더 이상 필요 없는 임시 리소스 — 즉시 GPU 리소스 반환.
		// (풀 GC는 무겁고 게임 스레드를 오래 막을 수 있어 쓰지 않는다. ReleaseResource + Flush로 충분.)
		HighResTarget->ReleaseResource();
		FlushRenderingCommands();
		HighResTarget->MarkAsGarbage();
		HighResTarget = nullptr;
	}

	if (!bReadOk)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MapCapture] 캡처 렌더타겟 ReadPixels 실패 — BakedMapTexture 갱신 스킵."));
		return;
	}

	// Factor == 1이어도 동일 경로(박스필터 1x1 = 단순 복사)로 BakedMapTexture까지 갱신한다.
	TArray<FColor> DownsampledPixels;
	int32 DstW = 0, DstH = 0;
	DownsamplePixelsBoxFilter(CapturedPixels, FinalSizeX * Factor, FinalSizeY * Factor, Factor, DownsampledPixels, DstW, DstH);

	if (!MapConfig || !MapConfig->BakedMapTexture)
	{
		UE_LOG(LogTemp, Warning, TEXT("[MapCapture] MapConfig->BakedMapTexture가 없어 캡처 결과를 반영하지 못했습니다."));
		return;
	}

	UTexture2D* Tex = MapConfig->BakedMapTexture;
	Tex->Source.Init(DstW, DstH, 1, 1, TSF_BGRA8, reinterpret_cast<const uint8*>(DownsampledPixels.GetData()));
	Tex->UpdateResource();
	Tex->PostEditChange();
	Tex->MarkPackageDirty();

	UE_LOG(LogTemp, Log,
		TEXT("[MapCapture] 캡처 완료 → BakedMapTexture 갱신 (%dx%d, x%d 슈퍼샘플)"),
		DstW, DstH, Factor);
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

	// 슈퍼샘플 캡처: 선명한 맵 설정(안개/노출/후처리 정리) + 광원 처리 + 다운샘플까지 내부에서 수행.
	CaptureWithSupersampling();

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