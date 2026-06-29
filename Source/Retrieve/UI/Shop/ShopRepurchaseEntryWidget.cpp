#include "UI/Shop/ShopRepurchaseEntryWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UI/Shop/ShopPanelWidget.h"

void UShopRepurchaseEntryWidget::InitEntry(int32 InHistoryIndex, const FText& InItemName,
                                           const FText& InPriceText, UShopPanelWidget* InOwnerPanel,
                                           bool bCanRepurchase)
{
	HistoryEntryIndex = InHistoryIndex;
	OwnerPanel        = InOwnerPanel;

	if (Text_ItemName)
	{
		Text_ItemName->SetText(InItemName);
	}
	if (Text_Price)
	{
		Text_Price->SetText(InPriceText);
	}

	if (Button_Repurchase)
	{
		Button_Repurchase->OnClicked.RemoveDynamic(this, &UShopRepurchaseEntryWidget::HandleRepurchaseClicked);
		Button_Repurchase->OnClicked.AddDynamic(this, &UShopRepurchaseEntryWidget::HandleRepurchaseClicked);
		Button_Repurchase->SetIsEnabled(bCanRepurchase);
	}
}

void UShopRepurchaseEntryWidget::HandleRepurchaseClicked()
{
	if (OwnerPanel)
	{
		OwnerPanel->ExecuteRepurchase(HistoryEntryIndex);
	}
}
