#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "RetrieveItemDescriptionHelper.generated.h"

class UDataTable;
struct FRetrieveWeaponDataRow;
struct FRetrieveConsumableItemRow;
struct FRetrieveMaterialItemRow;

/**
 * 순수 아이템 설명 텍스트 생성 유틸리티.
 * 인벤토리 컨텍스트(보유 수량, 장착 여부, 퀵슬롯)는 포함하지 않는다.
 * CraftPanel, InventoryPanel 툴팁 등 여러 위젯에서 재사용한다.
 */
UCLASS()
class RETRIEVE_API URetrieveItemDescriptionHelper : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * ItemId + CategoryTag 기반으로 아이템 설명 FText를 반환한다.
	 * CategoryTag가 매칭되는 테이블이 없으면 FText::GetEmpty()를 반환한다.
	 * 방어구(Item.Armor) 연동은 해당 DataTable 완성 후 파라미터 추가 예정.
	 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Item")
	static FText BuildItemDescription(
		FName ItemId,
		FGameplayTag CategoryTag,
		UDataTable* ConsumableTable,
		UDataTable* MaterialTable,
		UDataTable* WeaponTable
	);

private:
	static FText FormatWeapon(const FRetrieveWeaponDataRow& Row);
	static FText FormatConsumable(const FRetrieveConsumableItemRow& Row);
	static FText FormatMaterial(const FRetrieveMaterialItemRow& Row);
	static FString GetTagLeaf(FGameplayTag Tag);
};
