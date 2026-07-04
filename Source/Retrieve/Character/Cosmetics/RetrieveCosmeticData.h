
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "RetrieveCosmeticAnimationTypes.h"
#include "RetrieveModularMeshTypes.h"
#include "RetrieveCosmeticData.generated.h"

class UNiagaraSystem;

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

	// ---- Hand VFX (스태프 왼손 원소 이펙트) ----
	/** 왼손 VFX 부착 소켓/본 이름. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic|Hand VFX")
	FName HandVFXSocket = FName("Hand_Magic");

	/** 원소 태그 → 그 원소일 때 왼손에 부착할 Niagara(원소별 개별 시스템). Staff 장착 중에만 표시. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic|Hand VFX", meta = (Categories = "Element"))
	TMap<FGameplayTag, TSoftObjectPtr<UNiagaraSystem>> HandVFXByElement;

	// TODO: MorphTarget 연결 예약, 성별에 따른 모프타겟 조절
};
