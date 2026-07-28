#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "ShopSellSlotWidget.generated.h"

class UImage;
class UTextBlock;
class UBorder;
class UShopPanelWidget;
class UTexture2D;

/**
 * 판매 탭 개별 아이템 슬롯.
 * - 선택 오버레이 표시 (Border_SelectOverlay)
 * - 마우스 이벤트를 OwnerPanel에 전달하여 다중 선택 / shift+drag 처리
 * - SetSlotData() 시 WBP_ItemDetailTooltip.SetDetailInfo() 자동 주입
 */
UCLASS()
class RETRIEVE_API UShopSellSlotWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitSlot(int32 InIndex, UShopPanelWidget* InOwner);

	void SetSlotData(FName InItemId, FGameplayTag InCategoryTag, int32 InQuantity,
	                 UTexture2D* InIcon, bool bInEquipped = false, int32 InUnitSellPrice = 0);

	/** 선택 상태 표시. bSelected=카트에 담김, bActive=수량 조절 대상, InChosenQty=판매 예정 수량(>=0이면 수량 텍스트로 표시). */
	void SetSelected(bool bSelected, bool bActive = false, int32 InChosenQty = -1);
	void ClearSlot();

	void ApplyMinimumSize(const FVector2D& MinSize) { SetMinimumDesiredSize(MinSize); }

	FName           GetItemId()       const { return ItemId; }
	FGameplayTag    GetCategoryTag()  const { return ItemCategoryTag; }
	int32           GetQuantity()     const { return ItemQuantity; }
	int32           GetSlotIndex()    const { return SlotIndex; }
	bool            IsEquipped()      const { return bIsEquipped; }

protected:
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry,
	                                       const FPointerEvent& InMouseEvent) override;
	virtual void   NativeOnMouseEnter(const FGeometry& InGeometry,
	                                  const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry,
	                                     const FPointerEvent& InMouseEvent) override;

	/** 아이템 아이콘 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Icon;

	/** 우하단 수량 텍스트 (1개이면 숨김) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Count;

	/** 개당 판매가 표시. 이름 "Text_Price"로 WBP에 배치. 없으면 무시. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Price;

	/** 선택 시 표시되는 강조 오버레이 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_SelectOverlay;

	/** 장착 중인 장비 표시용(예: "장착" 배지). 이름 "Text_Equipped"로 WBP에 배치. 없으면 무시. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Equipped;

private:
	int32        SlotIndex     = -1;
	bool         bIsSelected   = false;
	bool         bIsEquipped   = false;
	FName        ItemId;
	FGameplayTag ItemCategoryTag;
	int32        ItemQuantity  = 0;

	UPROPERTY()
	TObjectPtr<UShopPanelWidget> OwnerPanel;
};
