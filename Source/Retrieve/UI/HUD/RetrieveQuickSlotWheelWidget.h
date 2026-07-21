#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Inventory/InventoryComponent.h"
#include "RetrieveQuickSlotWheelWidget.generated.h"

class UInventoryComponent;
class UDataTable;
class UUserWidget;
class UButton;

UENUM(BlueprintType)
enum class EQuickSlotWheelMode : uint8
{
	Use,
	Assign
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FQuickSlotWheelSlotClickedSignature,
	int32, SlotKey
);

UCLASS()
class RETRIEVE_API URetrieveQuickSlotWheelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "QuickSlotWheel")
	FQuickSlotWheelSlotClickedSignature OnWheelSlotClicked;

	UFUNCTION(BlueprintCallable, Category = "QuickSlotWheel")
	void InitializeQuickSlotWheel(UInventoryComponent* InInventoryComponent, UDataTable* InItemIconTable);

	UFUNCTION(BlueprintCallable, Category = "QuickSlotWheel")
	void RefreshFromQuickSlots();

	UFUNCTION(BlueprintCallable, Category = "QuickSlotWheel")
	void OpenForAssign(FName InPendingItemId, FGameplayTag InPendingCategoryTag);

	UFUNCTION(BlueprintCallable, Category = "QuickSlotWheel")
	void OpenForUse();

	// 휠 닫기 (ESC / 외부 호출)
	UFUNCTION(BlueprintCallable, Category = "QuickSlotWheel")
	void CloseWheel();

	// 라디얼 조작: 현재 마우스 방향으로 하이라이트된 슬롯을 사용하고 닫는다 (T 뗄 때 호출)
	UFUNCTION(BlueprintCallable, Category = "QuickSlotWheel")
	void ActivateHighlightedSlotAndClose();

	// 슬롯별 클릭 핸들러 — UMG 이벤트에서 직접 바인딩
	UFUNCTION(BlueprintCallable, Category = "QuickSlotWheel")
	void HandleItem00Clicked();

	UFUNCTION(BlueprintCallable, Category = "QuickSlotWheel")
	void HandleItem01Clicked();

	UFUNCTION(BlueprintCallable, Category = "QuickSlotWheel")
	void HandleItem02Clicked();

	UFUNCTION(BlueprintCallable, Category = "QuickSlotWheel")
	void HandleItem03Clicked();

	UFUNCTION(BlueprintCallable, Category = "QuickSlotWheel")
	void HandleItem04Clicked();

	UFUNCTION(BlueprintCallable, Category = "QuickSlotWheel")
	void HandleItem05Clicked();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	UPROPERTY()
	TObjectPtr<UInventoryComponent> InventoryComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuickSlotWheel")
	TObjectPtr<UDataTable> ItemIconTable;

	UPROPERTY(BlueprintReadOnly, Category = "QuickSlotWheel")
	EQuickSlotWheelMode WheelMode = EQuickSlotWheelMode::Use;

	UPROPERTY(BlueprintReadOnly, Category = "QuickSlotWheel")
	FName PendingAssignItemId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "QuickSlotWheel")
	FGameplayTag PendingAssignCategoryTag;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UUserWidget> Item_00;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UUserWidget> Item_01;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UUserWidget> Item_02;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UUserWidget> Item_03;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UUserWidget> Item_04;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UUserWidget> Item_05;

	// 인벤토리 등록(Assign) 모드에서만 보이는 닫기 버튼. 인게임 T키(Use) 모드에서는 숨긴다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_CloseWheel;

	UFUNCTION()
	void HandleQuickSlotChanged(int32 SlotKey, FRetrieveQuickSlotEntry Entry);

	UFUNCTION()
	void HandleCloseWheelClicked();

private:
	void HandleWheelSlotClicked(int32 SlotKey);
	UUserWidget* GetSlotWidget(int32 SlotKey) const;

	// 각 슬롯 위젯 내부의 "BUTTON"을 찾아 클릭 핸들러에 바인딩
	void BindSlotButtons();
	class UButton* GetSlotButton(UUserWidget* SlotWidget) const;
	bool bSlotButtonsBound = false;

	// 라디얼 방향 선택
	int32 HighlightedSlotKey = INDEX_NONE;
	void UpdateRadialHighlight(const FGeometry& MyGeometry);
	int32 ComputeSlotFromMouse(const FGeometry& MyGeometry) const;
	void SetSlotHighlighted(int32 SlotKey, bool bHighlighted);

	void ApplyItemToWheelSlot(UUserWidget* SlotWidget, const FRetrieveQuickSlotEntry& Entry);
	void ClearWheelSlot(UUserWidget* SlotWidget);

	void SetObjectProperty(UUserWidget* TargetWidget, FName PropertyName, UObject* Value);
	void SetBoolProperty(UUserWidget* TargetWidget, FName PropertyName, bool bValue);
	void SetIntProperty(UUserWidget* TargetWidget, FName PropertyName, int32 Value);
	void SetLinearColorProperty(UUserWidget* TargetWidget, FName PropertyName, const FLinearColor& Value);
	void SetVector2DProperty(UUserWidget* TargetWidget, FName PropertyName, const FVector2D& Value);
	void SetMarginProperty(UUserWidget* TargetWidget, FName PropertyName, const FMargin& Value);

	void CallSlotRefreshFunction(UUserWidget* SlotWidget);
};
