#pragma once

#include "CoreMinimal.h"
#include "Data/Interaction/RetrieveInteractionResultAsset.h"
#include "GameplayTagContainer.h"
#include "RetrievePickupGroupResultAsset.generated.h"

/**
 * 한 묶음에 여러 아이템이 들어가는 픽업 엔트리.
 * (보물상자, 퀘스트 보상 등의 "고정 다중 보상" 표현에 사용)
 */
USTRUCT(BlueprintType)
struct RETRIEVE_API FRetrievePickupEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Pickup")
	FName ItemId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Pickup",
		meta = (Categories = "Item"))
	FGameplayTag ItemCategoryTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Pickup",
		meta = (ClampMin = "1"))
	int32 Quantity = 1;
};

/**
 * 여러 아이템을 한 번에 인벤토리에 추가하는 결과 DataAsset.
 *
 * 사용 시나리오:
 *   - 보물상자: 골드 100 + 회복약 2 + 재료 1
 *   - 퀘스트 보상: 무기 1 + 재료 5
 *   - 시작 인벤토리: 기본 아이템 한 묶음
 *
 * Items 배열에 각 아이템 엔트리를 추가하면 ApplyResult가 모두 순회하면서
 * InventoryComponent.AddItem을 호출한다.
 *
 * 보상 묶음을 별도 DataAsset으로 분리해두면 같은 묶음을 여러 액터/이벤트에서 재사용 가능
 * (예: DA_RewardBundle_BasicChest를 BP_TreasureChest_Common 인스턴스 여러 개가 공유).
 */
UCLASS(BlueprintType)
class RETRIEVE_API URetrievePickupGroupResultAsset : public URetrieveInteractionResultAsset
{
	GENERATED_BODY()

public:
	/** 한 번에 적용될 아이템 묶음 (배열 순서대로 인벤토리에 추가) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Pickup")
	TArray<FRetrievePickupEntry> Items;

	virtual void ApplyResult_Implementation(
		UObject* WorldContextObject,
		AActor* Instigator,
		AActor* InteractedActor) const override;
};
