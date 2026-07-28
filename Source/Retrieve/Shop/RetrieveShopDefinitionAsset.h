#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Shop/RetrieveShopTypes.h"
#include "RetrieveShopDefinitionAsset.generated.h"

/** 상점 NPC 1개의 특성 전부를 담은 DataAsset.
 *  BP_ShopNPC의 ShopComponent.ShopDefinition 슬롯에 드래그&드롭으로 할당. */
UCLASS(BlueprintType)
class RETRIEVE_API URetrieveShopDefinitionAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 상점 표시 이름 / NPC 이름 (UI 타이틀 및 하단 대화창) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	FText ShopName = INVTEXT("상점");

	/** 상점 종류 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	ERetrieveShopType ShopType = ERetrieveShopType::General;

	/** FRetrieveShopItemRow rows — 이 상점이 판매하는 아이템 목록 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	TObjectPtr<UDataTable> ShopItemTable;

	/** 플레이어 판매가 배율. BasePrice * SellPriceRate = 실제 판매가 (기본 50%) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SellPriceRate = 0.5f;

	/** 상점 NPC 인사말 — WBP_ShopPanel 하단 대화창 출력용 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	FText GreetingText = INVTEXT("어서오세요. 무엇을 찾으시나요?");

	/** 이 상점의 구매가 배율 (지역 물가 보정). 기본 1.0
	 *  예) 사막 상점 1.5 → 모든 아이템 50% 추가 가격 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop", meta = (ClampMin = "0.1"))
	float PriceMultiplier = 1.0f;

	/** 비어 있으면 ShopItemTable 전체 행을 표시.
	 *  값이 있으면 나열된 RowName만 표시 — 여러 DA가 같은 DT를 공유 가능. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	TArray<FName> ShopItemRowFilter;

	// ---- 순환 재고 (화톳불 사용 시 갱신)
	/** 순환 재고 풀 DataTable (FRetrieveShopItemRow 기반).
	 *  화톳불 사용 시 이 풀에서 RotatingSlotCount개를 무작위 선택해 진열. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop|Rotating")
	TObjectPtr<UDataTable> RotatingPoolTable;

	/** 순환 재고 슬롯 수. 0이면 순환 재고 비활성화. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop|Rotating", meta = (ClampMin = "0"))
	int32 RotatingSlotCount = 3;

	/** 비어 있으면 RotatingPoolTable 전체에서 뽑음.
	 *  값이 있으면 이 목록에서만 순환 재고를 뽑음 — 지역별 순환 풀 제한 가능. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop|Rotating")
	TArray<FName> RotatingRowFilter;
};
