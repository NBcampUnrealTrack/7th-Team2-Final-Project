#include "UI/Shop/ShopSellSlotWidget.h"
#include "UI/Shop/ShopPanelWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Blueprint/UserWidget.h"
#include "Input/Events.h"

void UShopSellSlotWidget::InitSlot(int32 InIndex, UShopPanelWidget* InOwner)
{
	SlotIndex  = InIndex;
	OwnerPanel = InOwner;
}

void UShopSellSlotWidget::SetSlotData(FName InItemId, FGameplayTag InCategoryTag, int32 InQuantity,
                                       UTexture2D* InIcon, bool bInEquipped, int32 InUnitSellPrice)
{
	ItemId          = InItemId;
	ItemCategoryTag = InCategoryTag;
	ItemQuantity    = InQuantity;
	bIsEquipped     = bInEquipped;

	// 개당 판매가 표시("N G"). 0이면 판매 불가/미가격 아이템이므로 "-"로 표기.
	if (Text_Price)
	{
		Text_Price->SetText(InUnitSellPrice > 0
			? FText::Format(INVTEXT("{0} G"), FText::AsNumber(InUnitSellPrice))
			: INVTEXT("-"));
	}

	if (Image_Icon)
	{
		if (InIcon) Image_Icon->SetBrushFromTexture(InIcon);
		else        Image_Icon->SetBrushFromTexture(nullptr);
	}
	if (Text_Count)
		Text_Count->SetText(InQuantity > 1 ? FText::AsNumber(InQuantity) : FText::GetEmpty());

	// 장착 중인 장비는 "장착" 배지를 노출한다(판매 방지 대상임을 시각적으로 구분).
	if (Text_Equipped)
	{
		Text_Equipped->SetText(INVTEXT("장착"));
		Text_Equipped->SetVisibility(bInEquipped
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}

	// 툴팁은 패널(UShopPanelWidget)이 아이콘/등급/스탯/가격까지 채워 SetToolTip으로 주입한다.
}

void UShopSellSlotWidget::SetSelected(bool bSelected, bool bActive, int32 InChosenQty)
{
	bIsSelected = bSelected;
	if (Border_SelectOverlay)
	{
		Border_SelectOverlay->SetVisibility(bSelected
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
		// 활성 슬롯은 더 진하게(불투명), 담긴 슬롯은 옅게 강조.
		Border_SelectOverlay->SetRenderOpacity(bActive ? 1.0f : 0.55f);
	}

	// 카트에 담긴 동안에는 "판매 예정 수량"을 수량 텍스트로 보여준다(안 담겼으면 보유량).
	if (Text_Count)
	{
		if (bSelected && InChosenQty >= 0)
		{
			Text_Count->SetText(FText::AsNumber(InChosenQty));
		}
		else
		{
			Text_Count->SetText(ItemQuantity > 1 ? FText::AsNumber(ItemQuantity) : FText::GetEmpty());
		}
	}
}

void UShopSellSlotWidget::ClearSlot()
{
	ItemId          = NAME_None;
	ItemCategoryTag = FGameplayTag::EmptyTag;
	ItemQuantity    = 0;
	bIsSelected     = false;
	bIsEquipped     = false;

	if (Image_Icon)             Image_Icon->SetBrushFromTexture(nullptr);
	if (Text_Count)             Text_Count->SetText(FText::GetEmpty());
	if (Text_Price)             Text_Price->SetText(FText::GetEmpty());
	if (Border_SelectOverlay)   Border_SelectOverlay->SetVisibility(ESlateVisibility::Collapsed);
	if (Text_Equipped)          Text_Equipped->SetVisibility(ESlateVisibility::Collapsed);
	SetToolTip(nullptr);
}

FReply UShopSellSlotWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry,
                                                     const FPointerEvent& InMouseEvent)
{
	if (OwnerPanel && InMouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		OwnerPanel->HandleSellSlotPressed(SlotIndex, InMouseEvent.IsShiftDown());
		return FReply::Handled();
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UShopSellSlotWidget::NativeOnMouseEnter(const FGeometry& InGeometry,
                                              const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	// 마우스 버튼이 눌린 상태로 진입 → 드래그 중
	if (OwnerPanel && InMouseEvent.GetPressedButtons().Contains(EKeys::LeftMouseButton))
	{
		OwnerPanel->HandleSellSlotEntered(SlotIndex);
	}
}

FReply UShopSellSlotWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry,
                                                   const FPointerEvent& InMouseEvent)
{
	if (OwnerPanel)
		OwnerPanel->EndDragSelect();
	return FReply::Handled();
}
