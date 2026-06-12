#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interface/RetrieveWaterProvider.h"
#include "RetrieveWaterProviderComponent.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class USceneComponent;

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
	virtual bool TryGetWaterColumn_Implementation(const FVector& Location, float& OutSurfaceZ) const override;
	virtual FVector GetFlowVelocity_Implementation(const FVector& Location) const override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	/** 물 영역 오버랩 트리거 박스. 미지정 시 WaterBoxTag로 자동 탐색. (수면 Z는 WaterSurfaceTag 메시 담당) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Water")
	TObjectPtr<UBoxComponent> WaterTriggerBox;

	/** WaterTriggerBox 미지정 시 이 태그를 가진 BoxComponent를 찾음(오버랩 전담). 박스 Component Tags에 추가. */
	UPROPERTY(EditAnywhere, Category = "Retrieve|Water")
	FName WaterBoxTag = TEXT("WaterTrigger");

	/** 수면 메시 태그 — 이 태그를 가진 컴포넌트의 Z가 수면. (강 스플라인 메시 등 확장점) 메시 Component Tags에 추가. */
	UPROPERTY(EditAnywhere, Category = "Retrieve|Water")
	FName WaterSurfaceTag = TEXT("WaterSurface");

	/** 수면 = 수면 메시 Z + 이 오프셋. 메시 미지정 시 액터 Z 폴백. */
	UPROPERTY(EditAnywhere, Category = "Retrieve|Water")
	float SurfaceZOffset = 0.f;

	/** WaterSurfaceTag로 해석한 수면 컴포넌트. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> ResolvedSurface;

	/** 평면 수역(호수)의 물살. 보통 0. 강은 별도 Provider가 담당. */
	UPROPERTY(EditAnywhere, Category = "Retrieve|Water")
	FVector FlowVelocity = FVector::ZeroVector;
};
