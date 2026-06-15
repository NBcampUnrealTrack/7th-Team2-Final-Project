#pragma once

#include "CoreMinimal.h"
#include "Components/SphereComponent.h"
#include "Interface/RetrieveWaterProvider.h"
#include "RetrieveCameraWaterProbeComponent.generated.h"

/**
 * 카메라 위치 기준 수역 감지 Probe.
 * Pawn/Capsule 수영 판정과 분리해서 수중 PP, WaterMask에 사용합니다.
 */
UCLASS(ClassGroup = "Retrieve", meta = (BlueprintSpawnableComponent))
class RETRIEVE_API URetrieveCameraWaterProbeComponent : public USphereComponent
{
	GENERATED_BODY()

public:
	URetrieveCameraWaterProbeComponent();

	const TScriptInterface<IRetrieveWaterProvider>& GetCurrentWater() const { return CurrentWater; }

	bool ResolveCurrentWater(const FVector& QueryLocation, float& OutSurfaceZ);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void CheckInitialWaterOverlap();
	void RegisterWaterProvidersFromActor(const AActor* Actor);
	void UnregisterWaterProvidersFromActor(const AActor* Actor);
	void RegisterWaterProvider(const TScriptInterface<IRetrieveWaterProvider>& InWater);
	void UnregisterWaterProvider(UObject* WaterObject);

	UPROPERTY(Transient)
	TScriptInterface<IRetrieveWaterProvider> CurrentWater;

	UPROPERTY(Transient)
	TArray<TScriptInterface<IRetrieveWaterProvider>> CandidateWaters;
};
