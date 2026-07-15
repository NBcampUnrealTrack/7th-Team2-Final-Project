#include "UI/Shop/ShopItemEntryWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

void UShopItemEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Entry)
	{
		Button_Entry->OnClicked.RemoveDynamic(this, &UShopItemEntryWidget::HandleClicked);
		Button_Entry->OnClicked.AddDynamic(this, &UShopItemEntryWidget::HandleClicked);
	}
}

void UShopItemEntryWidget::SetupBuyEntry(FName InRowName, const FRetrieveShopItemRow& Row,
                                         FText DisplayName, UTexture2D* Icon)
{
	RowName = InRowName;

	if (Text_ItemName)
	{
		Text_ItemName->SetText(DisplayName);
	}

	if (Text_Stock)
	{
		Text_Stock->SetText(Row.StockCount < 0
			? FText::FromString(TEXT("\u221E"))
			: FText::AsNumber(Row.StockCount));
	}

	if (Text_Price)
	{
		Text_Price->SetText(FText::AsNumber(Row.BuyPrice));
	}

	if (Image_Icon && Icon)
	{
		Image_Icon->SetBrushFromTexture(Icon);
	}

	SetSelected(false);
}

void UShopItemEntryWidget::SetRuntimeStock(int32 Remaining)
{
	if (Text_Stock)
	{
		Text_Stock->SetText(Remaining < 0
			? FText::FromString(TEXT("∞"))
			: FText::AsNumber(Remaining));
	}
}

void UShopItemEntryWidget::SetIsRotatingStock(bool bRotating)
{
	bIsRotatingStock = bRotating;
	if (Text_RotatingBadge)
	{
		Text_RotatingBadge->SetVisibility(bRotating
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}
}

void UShopItemEntryWidget::SetSelected(bool bSelected)
{
	if (Border_Selection)
	{
		Border_Selection->SetVisibility(bSelected
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed);
	}
}

void UShopItemEntryWidget::HandleClicked()
{
	OnEntryClicked.Broadcast(RowName);
}
