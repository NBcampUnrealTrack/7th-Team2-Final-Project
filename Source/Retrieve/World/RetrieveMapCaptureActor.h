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

#if WITH_EDITOR
	UFUNCTION(CallInEditor, Category="Retrieve|MapCapture")
	void RefreshMapAll();
#endif

private:
#if WITH_EDITOR
	bool CalculateTaggedBounds(FBox& OutBounds) const;
	void UpdateMapConfig(UTexture2D* SavedTexture, const FVector2D& Origin, const FVector2D& ExtentXY);
#endif
};