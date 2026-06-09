#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "RetrieveElementUILibrary.generated.h"

/**
 * 원소 UI 전용 헬퍼 라이브러리.
 * MVVM View Bindings Conversion Function으로 사용된다.
 */
UCLASS()
class RETRIEVE_API URetrieveElementUILibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * GameplayTag(Element.Fire/Water/Wind) → LinearColor 변환.
	 * WBP_ElementGauge View Bindings의 FillColorAndOpacity Conversion Function으로 사용.
	 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|UI|Element",
		meta = (DisplayName = "Element Tag To Color"))
	static FLinearColor ElementTagToColor(FGameplayTag ElementTag);
};
