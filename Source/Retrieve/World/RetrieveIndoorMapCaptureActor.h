#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RetrieveIndoorMapCaptureActor.generated.h"

class ARetrieveMinimapAreaVolume;
class USceneCaptureComponent2D;
class URetrieveMinimapAreaDataAsset;
class UTextureRenderTarget2D;

/** One always-loaded capture service shared by all indoor minimap areas. */
UCLASS(BlueprintType)
class RETRIEVE_API ARetrieveIndoorMapCaptureActor : public AActor
{
	GENERATED_BODY()

public:
	ARetrieveIndoorMapCaptureActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Retrieve|IndoorMap")
	TObjectPtr<USceneCaptureComponent2D> SceneCapture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Retrieve|IndoorMap")
	TObjectPtr<UTextureRenderTarget2D> SharedRenderTarget;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Retrieve|IndoorMap", meta=(ClampMin="100.0"))
	float CaptureHeight = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Retrieve|IndoorMap", meta=(ClampMin="0.0"))
	float CaptureDelay = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Retrieve|IndoorMap|Auto Setup")
	TObjectPtr<URetrieveMinimapAreaDataAsset> DefaultAutoAreaData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Retrieve|IndoorMap|Auto Setup")
	TArray<FString> IndoorNamePatterns;

	void RequestCapture(ARetrieveMinimapAreaVolume* Area);

	/** Creates area actors around named Interior/Dungeon/Cave packed-level actors. */
	UFUNCTION(CallInEditor, Category="Retrieve|IndoorMap|Auto Setup")
	void GenerateIndoorAreasFromNamedActors();

private:
	void CapturePendingArea();
	void ConfigureCapture();

	UPROPERTY(Transient)
	TObjectPtr<ARetrieveMinimapAreaVolume> PendingArea;

	UPROPERTY(Transient)
	TObjectPtr<ARetrieveMinimapAreaVolume> LastCapturedArea;

	FTimerHandle CaptureTimerHandle;
};
