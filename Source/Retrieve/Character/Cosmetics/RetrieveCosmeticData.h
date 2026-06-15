
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "RetrieveCosmeticAnimationTypes.h"
#include "RetrieveModularMeshTypes.h"
#include "RetrieveCosmeticData.generated.h"

UCLASS(BlueprintType)
class RETRIEVE_API URetrieveCosmeticData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic|Animations Layer")
	FRetrieveAnimLayerSelectionSet AnimLayerRules;

	/** 초기화 시 CosmeticTags에 기본으로 추가되는 태그. 성별 등 불변 속성.
	 *  예: Gender.Female */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic|Defaults")
	FGameplayTagContainer DefaultCosmeticTags;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic|Visual Mesh")
	FRetrieveVisualLayoutSelectionSet VisualLayoutRules;

	// TODO: MorphTarget 연결 예약, 성별에 따른 모프타겟 조절
};
