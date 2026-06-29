#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ShopRepurchaseEntryWidget.generated.h"

class UTextBlock;
class UButton;
class UShopPanelWidget;

/**
 * 재구매 목록 단일 항목.
 * InitEntry()에서 데이터를 받고, Button_Repurchase 클릭 시 OwnerPanel->ExecuteRepurchase() 호출.
 */
UCLASS()
class RETRIEVE_API UShopRepurchaseEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitEntry(int32 InHistoryIndex, const FText& InItemName,
	               const FText& InPriceText, UShopPanelWidget* InOwnerPanel,
	               bool bCanRepurchase = true);

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ItemName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Price;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Repurchase;

private:
	int32 HistoryEntryIndex = -1;

	UPROPERTY()
	TObjectPtr<UShopPanelWidget> OwnerPanel;

	UFUNCTION()
	void HandleRepurchaseClicked();
};
