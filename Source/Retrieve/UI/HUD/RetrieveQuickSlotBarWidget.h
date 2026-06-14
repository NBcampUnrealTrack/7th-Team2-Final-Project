#pragma once

#include "CoreMinimal.h"
#include "UI/VFX/RetrieveUIVFXWidget.h"
#include "RetrieveQuickSlotBarWidget.generated.h"

class URetrieveQuickSlotEntryWidget;
class UInventoryComponent;
class UDataTable;

/** Displays the two combat consumable slots owned by InventoryComponent. */
UCLASS()
class RETRIEVE_API URetrieveQuickSlotBarWidget : public URetrieveUIVFXWidget
{
	GENERATED_BODY()

public:
	/** Refreshes item icons and counts for both quick slots. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|QuickSlot")
	void RefreshSlots();

	/** Icon table with FRetrieveItemIconRow rows. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|QuickSlot")
	TObjectPtr<UDataTable> QuickSlotIconTable;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION()
	void HandleConsumableSlotChanged(int32 InSlotKey, FName ItemId);

	UFUNCTION()
	void HandleSlotUsed(int32 SlotKey);

	void ResolveInventoryComponent();
	void ResolveSlotEntries();
	void RefreshSlotEntry(URetrieveQuickSlotEntryWidget* Entry, int32 SlotKey) const;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|QuickSlot", meta = (BindWidgetOptional))
	TObjectPtr<URetrieveQuickSlotEntryWidget> WBP_QuickSlotEntry_4;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|QuickSlot", meta = (BindWidgetOptional))
	TObjectPtr<URetrieveQuickSlotEntryWidget> WBP_QuickSlotEntry_5;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|QuickSlot")
	TObjectPtr<UInventoryComponent> InventoryComp;

};
