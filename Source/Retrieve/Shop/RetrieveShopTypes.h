#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "RetrieveShopTypes.generated.h"

UENUM(BlueprintType)
enum class ERetrieveShopType : uint8
{
	General  UMETA(DisplayName = "잡화 상점"),
	Weapon   UMETA(DisplayName = "무기 상점"),
	Armor    UMETA(DisplayName = "방어구 상점"),
};

/** DT_ShopInventory_* 의 Row. RowName == 상점 내 고유 슬롯 ID */
USTRUCT(BlueprintType)
struct RETRIEVE_API FRetrieveShopItemRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 아이템 DataTable RowName (무기/소모품/재료/방어구 DT의 ItemId) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	FName ItemId = NAME_None;

	/** Item.Weapon / Item.Consumable / Item.Material / Item.Armor */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop", meta = (Categories = "Item"))
	FGameplayTag ItemCategoryTag;

	/** 이 상점에서의 구매가. 0이면 무료 구매 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop", meta = (ClampMin = "0"))
	int32 BuyPrice = 100;

	/** -1 = 무제한 재고. 0 이상 = 구매 가능 최대 수량 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop", meta = (ClampMin = "-1"))
	int32 StockCount = -1;

	/** 목록 표시 순서 (낮을수록 위) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	int32 SortOrder = 0;
};

/** 상점 1곳의 소진 재고 스냅샷. 세이브 파일에 직렬화됨.
 *  TMap<FName, FRetrieveShopStockSave>의 값으로 쓰기 위해 TMap을 구조체로 감쌌다
 *  (UPROPERTY는 TMap<Key, TMap<...>> 형태를 직접 허용하지 않음). */
USTRUCT(BlueprintType)
struct RETRIEVE_API FRetrieveShopStockSave
{
	GENERATED_BODY()

	/** 상점 슬롯 RowName → 남은 재고. 무한(-1) 재고 행은 저장하지 않는다. */
	UPROPERTY(BlueprintReadWrite)
	TMap<FName, int32> RemainingStock;
};

/** 판매 완료 후 재구매 히스토리 항목. 세이브 파일에 직렬화됨 */
USTRUCT(BlueprintType)
struct RETRIEVE_API FShopRepurchaseRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite) FName ItemId;
	UPROPERTY(BlueprintReadWrite) FGameplayTag ItemCategoryTag;
	/** 판매한 수량 */
	UPROPERTY(BlueprintReadWrite) int32 Quantity = 1;
	/** 재구매 시 지불할 금액 (판매 당시 가격 그대로) */
	UPROPERTY(BlueprintReadWrite) int32 RepurchasePrice = 0;
};
