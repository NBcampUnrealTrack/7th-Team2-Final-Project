
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RetrieveWaterProvider.generated.h"

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
	
	/**
	 * 위치 별 물살 속도(cm/s)
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Retrieve|Water")
	FVector GetFlowVelocity(const FVector& Location) const;
};
