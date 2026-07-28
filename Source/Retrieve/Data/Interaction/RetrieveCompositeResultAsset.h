#pragma once

#include "CoreMinimal.h"
#include "Data/Interaction/RetrieveInteractionResultAsset.h"
#include "RetrieveCompositeResultAsset.generated.h"

/**
 * 여러 ResultAsset을 하나의 결과로 묶는 DataAsset.
 *
 * 보통은 ResponseComponent.ResultAssets Array가 같은 역할을 하지만,
 * "재사용 가능한 묶음 자체"를 별도 DataAsset으로 분리하고 싶을 때 사용한다.
 *
 * 사용 시나리오:
 *   - 퀘스트 보상 = [PickupGroup(아이템) + LootResult(추가 보너스) + QuestProgress(향후)]
 *     → DA_QuestReward_001 한 개로 묶고, 여러 액터/이벤트에서 재사용
 *   - 보스 처치 보상 = [LootResult(드롭) + Composite(이벤트 트리거 + 사운드)]
 *
 * 재귀 가드:
 *   ChildResults에 자기 자신을 넣으면 무한 호출이 발생할 수 있어
 *   ApplyResult가 nullptr 체크 + 자기 자신 참조 검사를 한다.
 */
UCLASS(BlueprintType)
class RETRIEVE_API URetrieveCompositeResultAsset : public URetrieveInteractionResultAsset
{
	GENERATED_BODY()

public:
	/** 순서대로 ApplyResult가 호출될 자식 결과 목록 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Composite")
	TArray<TObjectPtr<URetrieveInteractionResultAsset>> ChildResults;

	virtual void ApplyResult_Implementation(
		UObject* WorldContextObject,
		AActor* Instigator,
		AActor* InteractedActor) const override;
};
