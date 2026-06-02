#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "RetrieveQuickSlotEntryWidget.generated.h"

class UImage;
class UTextBlock;
class UDataTable;

/** Displays one quick-slot icon and its stack count. */
UCLASS()
class RETRIEVE_API URetrieveQuickSlotEntryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 슬롯 초기화 — 슬롯키 / 아이템ID / 아이콘 테이블을 한 번에 설정하고 갱신 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|QuickSlot")
	void InitSlot(int32 InSlotKey, FName InItemId, UDataTable* InIconTable);

	/** 아이템 아이콘 갱신. ItemId가 None이면 아이콘 숨김 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|QuickSlot")
	void RefreshSlot(FName ItemId);

	/** 수량 텍스트 갱신. Count <= 1이면 텍스트 숨김 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|QuickSlot")
	void RefreshCount(int32 Count);

	UFUNCTION(BlueprintPure, Category = "Retrieve|QuickSlot")
	FName GetCurrentItemId() const { return NativeCurrentItemId; }

	UFUNCTION(BlueprintPure, Category = "Retrieve|QuickSlot")
	int32 GetSlotKey() const { return NativeSlotKey; }

	/** 아이콘 조회용 DataTable (Row: FRetrieveItemIconRow). InitSlot으로도 주입 가능 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|QuickSlot")
	TObjectPtr<UDataTable> QuickSlotIconTable;

protected:
	virtual void NativeConstruct() override;

	void ShowEmptySlot();

	// 디자이너 위젯 자동 바인딩 (이름 일치 시 컴파일 타임 연결)
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|QuickSlot", meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_Icon;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|QuickSlot", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Count;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|QuickSlot")
	FName NativeCurrentItemId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|QuickSlot")
	int32 NativeSlotKey = 0;

	FSlateBrush EmptySlotBrush;
	bool bHasCachedEmptySlotBrush = false;
};
