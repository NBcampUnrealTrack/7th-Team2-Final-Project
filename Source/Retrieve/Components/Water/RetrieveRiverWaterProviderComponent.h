#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interface/RetrieveWaterProvider.h"
#include "RetrieveRiverWaterProviderComponent.generated.h"

class UBoxComponent;
class USplineComponent;
class UPostProcessComponent;

/**
 * 강 스플라인에 얹는 수영 연동 Provider. 같은 액터의 USplineComponent를 샘플.
 * BeginPlay에서 스플라인 바운드로 AABB 박스 auto-fit(오버랩=진입/이탈).
 * TryGetWaterColumn은 스플라인 로컬프레임으로 폭/깊이 채널 판정(곡선·나선 대응).
 */
UCLASS(ClassGroup="Retrieve", meta=(BlueprintSpawnableComponent))
class RETRIEVE_API URetrieveRiverWaterProviderComponent : public UActorComponent, public IRetrieveWaterProvider
{
	GENERATED_BODY()

public:
	URetrieveRiverWaterProviderComponent();

	//~ IRetrieveWaterProvider
	virtual float GetWaterSurfaceZ_Implementation(const FVector& Location) const override;
	virtual bool TryGetWaterColumn_Implementation(const FVector& Location, float& OutSurfaceZ) const override;
	virtual FVector GetFlowVelocity_Implementation(const FVector& Location) const override;
	virtual FRetrieveWaterPPMaterials GetWaterPostProcessMaterials_Implementation() const override;	
	
	UPROPERTY(EditAnywhere, Category = "Retrieve|River")
	int32 BiomeOverride = -1; // -1  강 BP의 River Material Preset 자동, 그 외엔 강제함
	
protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& Sweep);
	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UPROPERTY(EditAnywhere, Category = "Retrieve|River")
	float HalfWidth = 500.f;
	UPROPERTY(EditAnywhere, Category = "Retrieve|River")
	float Depth = 200.f;
	UPROPERTY(EditAnywhere, Category = "Retrieve|River")
	float SurfaceMargin = 30.f;
	UPROPERTY(EditAnywhere, Category = "Retrieve|River")
	float SurfaceZOffset = 0.f;
	UPROPERTY(EditAnywhere, Category = "Retrieve|River")
	float FlowStrength = 0.f;

private:
	UPROPERTY(Transient)
	TObjectPtr<USplineComponent> Spline;

	UPROPERTY(Transient)
	TObjectPtr<UBoxComponent> WaterBox;

	UPROPERTY(Transient)
	TObjectPtr<UPostProcessComponent> UnderwaterPP; // 박스 바운드 수중 PP (원형 방식)

	int32 ReadBiomeIndex() const;
};
