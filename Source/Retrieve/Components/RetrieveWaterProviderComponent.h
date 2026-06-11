#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interface/RetrieveWaterProvider.h"
#include "RetrieveWaterProviderComponent.generated.h"

class UBoxComponent;
class UPrimitiveComponent;

/**
 * 기존 물 BP(StylizedWater 등)에 얹는 수영 연동 컴포넌트.
 * 지정한 트리거 박스의 오버랩 → 폰의 SwimDetectionComponent에 진입/이탈 통보.
 * 수면 Z/물살을 IRetrieveWaterProvider로 제공(수면 = 박스 윗면).
 * 박스는 마켓 에셋 것(TriggerWaterBody)을 그대로 재활용 — 우리 코드만 얹힘.
 */
UCLASS(ClassGroup="Retrieve", meta=(BlueprintSpawnableComponent))
class RETRIEVE_API URetrieveWaterProviderComponent : public UActorComponent, public IRetrieveWaterProvider
{
	GENERATED_BODY()

public:
	URetrieveWaterProviderComponent();

	//~ IRetrieveWaterProvider
	virtual float GetWaterSurfaceZ_Implementation(const FVector& Location) const override;
	virtual FVector GetFlowVelocity_Implementation(const FVector& Location) const override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** 물 영역 트리거 박스. BP에서 TriggerWaterBody 지정. 윗면 = 수면. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Water")
	TObjectPtr<UBoxComponent> WaterTriggerBox;

	/** 평면 수역(호수)의 물살. 보통 0. 강은 별도 Provider가 담당. */
	UPROPERTY(EditAnywhere, Category = "Retrieve|Water")
	FVector FlowVelocity = FVector::ZeroVector;
};
