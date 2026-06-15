
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RetrieveWaterProvider.generated.h"

class UMaterialInterface;

/** 수역이 제공하는 카메라 PP 머티리얼 세트. (Fog는 PP 아님 → 별도 메시) */
USTRUCT(BlueprintType)
struct FRetrieveWaterPPMaterials
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) 
	TObjectPtr<UMaterialInterface> Underwater = nullptr;
	
	UPROPERTY(BlueprintReadOnly) 
	TObjectPtr<UMaterialInterface> Waterline = nullptr;
};

UINTERFACE(BlueprintType)
class RETRIEVE_API URetrieveWaterProvider : public UInterface
{
	GENERATED_BODY()
};

class RETRIEVE_API IRetrieveWaterProvider
{
	GENERATED_BODY()

public:
	/**
	 * 위치 별 수면 월드 Z, 평면 = 상수, 강 = 스플라인 계산
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Retrieve|Water")
	float GetWaterSurfaceZ(const FVector& Location) const;

	/** 위치가 수중(채널 안)인지 + 수면 Z. 호수=항상 true, 강=스플라인 채널 판정. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Retrieve|Water")
	bool TryGetWaterColumn(const FVector& Location, float& OutSurfaceZ) const;

	/**
	 * 위치 별 물살 속도(cm/s)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Retrieve|Water")
	FVector GetFlowVelocity(const FVector& Location) const;
	
	/** 이 수역의 수중 PP 머티리얼. null이면 카메라 FX 미적용(예: 호수는 자체 볼륨). */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Retrieve|Water")
	FRetrieveWaterPPMaterials GetWaterPostProcessMaterials() const;
};
