#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RetrieveMapCaptureActor.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UTexture2D;
class URetrieveMapConfigDataAsset;
class URetrieveMapIconDataAsset;

UCLASS()
class RETRIEVE_API ARetrieveMapCaptureActor : public AActor
{
	GENERATED_BODY()

public:
	ARetrieveMapCaptureActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Retrieve|MapCapture")
	TObjectPtr<USceneCaptureComponent2D> SceneCapture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|MapCapture")
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|MapCapture")
	TObjectPtr<URetrieveMapConfigDataAsset> MapConfig;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|MapCapture")
	TObjectPtr<URetrieveMapIconDataAsset> WorldMapIconData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|MapCapture")
	FName BoundsIncludeTag = TEXT("MapBoundsInclude");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|MapCapture")
	float CaptureHeight = 50000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|MapCapture")
	float BoundsPadding = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|MapCapture")
	bool bUseSquareCaptureBounds = true;

	/**
	 * true: 언릿(평면 베이스컬러)으로 캡처 — 하늘/시간대/그림자 영향이 전혀 없어 가장 선명/일관.
	 * false: 라이팅 포함으로 캡처(지형 음영 유지). 단, 캡처 시점 레벨을 주간 스카이로 맞춰야 깔끔.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|MapCapture|Quality")
	bool bCaptureUnlit = true;

	/** 캡처 밝기 보정(EV). 결과가 어두우면 +, 너무 밝으면 - 로 조정. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|MapCapture|Quality")
	float CaptureExposureBias = 0.0f;

	// ── 라이팅 캡처용 자동 광원 오버라이드 (bCaptureUnlit=false 일 때만 적용) ──
	/** 캡처 동안 강제할 태양(디렉셔널) 방향. 각도를 주면 지형 입체감(음영)이 생긴다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|MapCapture|Quality", meta=(EditCondition="!bCaptureUnlit"))
	FRotator CaptureSunRotation = FRotator(-50.0f, -35.0f, 0.0f);

	/** 캡처 동안 강제할 태양 세기(lux). 결과가 어두우면 키운다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|MapCapture|Quality", meta=(EditCondition="!bCaptureUnlit"))
	float CaptureSunIntensity = 10.0f;

	/** 캡처 동안 강제할 태양 색(중립 흰색 권장). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|MapCapture|Quality", meta=(EditCondition="!bCaptureUnlit"))
	FLinearColor CaptureSunColor = FLinearColor::White;

	/** 캡처 동안 강제할 스카이라이트(그림자부 채움) 세기. 0이면 그림자가 어두워진다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|MapCapture|Quality", meta=(EditCondition="!bCaptureUnlit"))
	float CaptureSkyLightIntensity = 1.5f;

	/**
	 * 슈퍼샘플링 배수. 1보다 크면 (최종 해상도 × 배수) 크기의 임시 렌더타겟에 캡처한 뒤
	 * 박스필터로 최종 해상도로 다운샘플링해 BakedMapTexture에 반영한다 — 앤티에일리어싱 효과.
	 * 임시 렌더타겟은 캡처 직후 해제되므로 최종 텍스처 용량/런타임 메모리에는 영향이 없다.
	 * 1이면 슈퍼샘플링 없이 기존 방식대로 캡처.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Retrieve|MapCapture|Quality", meta=(ClampMin="1", ClampMax="8"))
	int32 SuperSampleFactor = 4;

#if WITH_EDITOR
	UFUNCTION(CallInEditor, Category="Retrieve|MapCapture")
	void RefreshMapAll();
#endif

private:
#if WITH_EDITOR
	bool CalculateTaggedBounds(FBox& OutBounds) const;
	void UpdateMapConfig(UTexture2D* SavedTexture, const FVector2D& Origin, const FVector2D& ExtentXY);

	/** 선명한 맵을 위해 캡처 직전 하늘/안개/자동노출/후처리를 끄고 노출을 고정한다. */
	void ConfigureCaptureForCleanMap();

	/**
	 * 캡처를 수행한다. 라이팅 모드(bCaptureUnlit=false)면 캡처 동안만 고정 주간 태양/스카이라이트를
	 * 강제했다가 같은 프레임 내에서 즉시 복원한다 → 레벨 시간대와 무관하게 일관된 음영.
	 */
	void CaptureSceneWithCleanLighting();

	/**
	 * SuperSampleFactor > 1이면 임시 고해상도 렌더타겟에 캡처 후 박스필터로 다운샘플링해
	 * MapConfig->BakedMapTexture에 직접 반영한다(앤티에일리어싱). 1이면 RenderTarget에 바로 캡처.
	 */
	void CaptureWithSupersampling();
#endif
};