#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UI/VFX/RetrieveUIVFXWidget.h"
#include "RetrieveItemPickupToastWidget.generated.h"

class UTextBlock;
class UWidget;

/**
 * WBP_ItemPickupToast의 C++ 베이스 클래스.
 *
 * Blueprint의 InitToast 함수 내 GetDataTableRow 노드에 DataTable 핀이
 * 미설정되어 빈 텍스트가 표시되는 문제를 C++에서 직접 DataTable을 조회하여
 * 수정한다. BindWidgetOptional로 ItemNameText / QuantityText TextBlock을
 * 자동 바인딩하므로 bIsVariable 여부와 무관하게 동작한다.
 */
UCLASS()
class RETRIEVE_API URetrieveItemPickupToastWidget : public URetrieveUIVFXWidget
{
	GENERATED_BODY()

public:
	/**
	 * 아이템 이름과 수량 텍스트를 DataTable에서 조회하여 TextBlock에 설정한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|HUD")
	void InitToast(FName ItemId, FGameplayTag ItemCategoryTag, int32 Quantity);

	/**
	 * 스택 내 슬롯 인덱스를 설정한다.
	 * HUD에서 토스트를 추가/제거할 때마다 호출해 겹침 없이 수직으로 정렬한다.
	 * @param SlotIndex  0 = 가장 위 슬롯
	 * @param SlotStartY 첫 번째 슬롯의 화면 상단으로부터의 Y 오프셋 (픽셀)
	 * @param SlotHeight 슬롯 하나의 높이 (토스트 높이 + 간격)
	 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|HUD")
	void SetToastSlotIndex(int32 SlotIndex, float SlotStartY = 100.f, float SlotHeight = 90.f);

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|UI VFX|Toast")
	TObjectPtr<UWidget> ToastVFXTarget;

	// 위젯 트리의 ItemNameText TextBlock에 자동 바인딩 (없으면 nullptr)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ItemNameText;

	// 위젯 트리의 QuantityText TextBlock에 자동 바인딩 (없으면 nullptr)
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QuantityText;

private:
	/** ItemCategoryTag를 기반으로 적절한 DataTable에서 DisplayName을 조회한다. */
	static FText LookupItemDisplayName(FName ItemId, FGameplayTag ItemCategoryTag);
};
