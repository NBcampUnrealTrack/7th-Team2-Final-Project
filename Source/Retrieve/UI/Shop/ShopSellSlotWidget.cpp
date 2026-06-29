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
                                       UTexture2D* InIcon, const FText& InDisplayName,
                                       const FText& InDescription, TSubclassOf<UUserWidget> TooltipClass)
{
	ItemId          = InItemId;
	ItemCategoryTag = InCategoryTag;
	ItemQuantity    = InQuantity;

	if (Image_Icon)
	{
		if (InIcon) Image_Icon->SetBrushFromTexture(InIcon);
		else        Image_Icon->SetBrushFromTexture(nullptr);
	}
	if (Text_Count)
		Text_Count->SetText(InQuantity > 1 ? FText::AsNumber(InQuantity) : FText::GetEmpty());

	// WBP_ItemDetailTooltip.SetDetailInfo(ItemName, ItemDetails) 주입
	if (TooltipClass)
	{
		UUserWidget* Tooltip = CreateWidget<UUserWidget>(this, TooltipClass);
		if (Tooltip)
		{
			if (UFunction* Func = Tooltip->FindFunction(FName("SetDetailInfo")))
			{
				struct FSetDetailInfoParams { FText ItemName; FText ItemDetails; };
				FSetDetailInfoParams Params{ InDisplayName, InDescription };
				Tooltip->ProcessEvent(Func, &Params);
			}
			SetToolTip(Tooltip);
		}
	}
}

void UShopSellSlotWidget::SetSelected(bool bSelected)
{
	bIsSelected = bSelected;
	if (Border_SelectOverlay)
	{
		Border_SelectOverlay->SetVisibility(bSelected
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}
}

void UShopSellSlotWidget::ClearSlot()
{
	ItemId          = NAME_None;
	ItemCategoryTag = FGameplayTag::EmptyTag;
	ItemQuantity    = 0;
	bIsSelected     = false;

	if (Image_Icon)             Image_Icon->SetBrushFromTexture(nullptr);
	if (Text_Count)             Text_Count->SetText(FText::GetEmpty());
	if (Border_SelectOverlay)   Border_SelectOverlay->SetVisibility(ESlateVisibility::Collapsed);
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
