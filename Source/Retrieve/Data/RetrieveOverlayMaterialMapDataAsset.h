#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "RetrieveOverlayMaterialMapDataAsset.generated.h"

class UMaterialInterface;

USTRUCT(BlueprintType)
struct FRetrieveOverlayEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Overlay")
	TObjectPtr<UMaterialInterface> Material = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Overlay")
	int32 Priority = 0;
};

UCLASS(BlueprintType, Const)
class RETRIEVE_API URetrieveOverlayMaterialMapDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 태그 하나에 대한 오버레이 엔트리 조회. 없으면 nullptr. */
	const FRetrieveOverlayEntry* Find(const FGameplayTag& Tag) const;

	/** 이 DA에 등록된 모든 키 태그를 수집. 컴포넌트가 리스너 등록 시 사용. */
	void GetKeyTags(FGameplayTagContainer& OutTags) const;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Overlay",
	          meta = (Categories = "State", TitleProperty = "Priority"))
	TMap<FGameplayTag, FRetrieveOverlayEntry> Map;
};
