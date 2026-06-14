#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CraftMaterialRowWidget.generated.h"

class UImage;
class UTextBlock;

/**
 * 제작 레시피 상세 패널의 재료 한 줄.
 *
 * WBP에서 다음 이름으로 위젯을 만들면 자동 바인딩된다:
 *   Image_MatIcon   — 재료 아이콘
 *   Text_MatName    — 재료 이름
 *   Text_MatCount   — "보유 3 / 필요 5" (부족 시 빨간색)
 */
UCLASS()
class RETRIEVE_API UCraftMaterialRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 재료 한 줄을 초기화한다.
	 * @param InIconTable  FRetrieveItemIconRow rows DataTable (아이콘 조회)
	 * @param InMatTable   FRetrieveMaterialItemRow rows DataTable (이름 조회)
	 * @param InItemId     재료 아이템 ID
	 * @param InRequired   레시피 요구 수량
	 * @param InOwned      현재 보유 수량
	 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Craft")
	void InitMaterialRow(
		UDataTable* InIconTable,
		UDataTable* InMatTable,
		FName InItemId,
		int32 InRequired,
		int32 InOwned);

	/** 보유량만 갱신 (재료 획득/사용 후 호출) */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Craft")
	void RefreshOwnedCount(int32 InOwned);

	UFUNCTION(BlueprintPure, Category = "Retrieve|Craft")
	bool IsSufficient() const { return OwnedCount >= RequiredCount; }

	UFUNCTION(BlueprintPure, Category = "Retrieve|Craft")
	FName GetMaterialItemId() const { return NativeItemId; }

protected:
	virtual void NativeConstruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_MatIcon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_MatName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_MatCount;

private:
	void ApplyCountColor();

	UPROPERTY()
	TObjectPtr<UDataTable> IconTable;

	UPROPERTY()
	TObjectPtr<UDataTable> MatTable;

	FName NativeItemId;
	int32 RequiredCount = 0;
	int32 OwnedCount = 0;

	static const FLinearColor SufficientColor;
	static const FLinearColor InsufficientColor;
};
