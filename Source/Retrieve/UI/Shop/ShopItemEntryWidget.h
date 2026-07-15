#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Shop/RetrieveShopTypes.h"
#include "ShopItemEntryWidget.generated.h"

class UTextBlock;
class UImage;
class UButton;
class UBorder;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FShopEntryClickedSignature, FName, RowName);

/** 구매 탭 목록의 각 행 위젯.
 *  레이아웃: [아이콘] [이름──────────] [재고] [⊙가격]
 *  WBP_ShopItemEntry 이름으로 BP 생성 후 ShopPanelWidget.ShopItemEntryWidgetClass 에 할당. */
UCLASS()
class RETRIEVE_API UShopItemEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Shop")
	void SetupBuyEntry(FName InRowName, const FRetrieveShopItemRow& Row,
	                   FText DisplayName, UTexture2D* Icon);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Shop")
	void SetSelected(bool bSelected);

	/** 순환 재고 여부를 설정하고 뱃지 위젯 가시성을 갱신합니다. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Shop")
	void SetIsRotatingStock(bool bRotating);

	/** 런타임 남은 재고로 재고 텍스트를 갱신한다(무한이면 -1 → "∞").
	 *  SetupBuyEntry가 DT의 정적 StockCount로 채운 값을 실제 잔량으로 덮어쓴다. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Shop")
	void SetRuntimeStock(int32 Remaining);

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Shop")
	bool bIsRotatingStock = false;

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Shop")
	FShopEntryClickedSignature OnEntryClicked;

protected:
	// WBP_ShopItemEntry 위젯 이름 일치 필수
	// HorizontalBox: Image_Icon(40x40) | Text_ItemName(Fill) | Text_Stock(80px) | [Image_CoinIcon | Text_Price(80px)]
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Icon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ItemName;

	/** 재고 수. -1이면 "∞" 표시 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Stock;

	/** 구매가. 재화 부족 시 빨간색 (BP에서 색상 바인딩) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Price;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_Selection;

	/** 순환 재고 표시 뱃지. WBP에서 이름 "Text_RotatingBadge"로 배치. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_RotatingBadge;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Entry;

	virtual void NativeConstruct() override;

private:
	UFUNCTION()
	void HandleClicked();

	FName RowName;
};
