#pragma once

#include "CoreMinimal.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameplayTagContainer.h"
#include "UI/RetrieveGamePanelWidget.h"
#include "InventoryPanelWidget.generated.h"

class UInventoryComponent;
class UWeaponComponent;
class UBorder;
class UButton;
class UHorizontalBox;
class UScrollBox;
class UTextBlock;
class UUniformGridPanel;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRetrieveInventoryWidgetSimpleSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRetrieveInventorySelectionChangedSignature, FName, ItemId, FGameplayTag, ItemCategoryTag);

UCLASS()
class RETRIEVE_API UInventoryPanelWidget : public URetrieveGamePanelWidget
{
	GENERATED_BODY()

public:
	UInventoryPanelWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Inventory|Data")
	TObjectPtr<UDataTable> WeaponDataTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Inventory|Data")
	TObjectPtr<UDataTable> ConsumableItemTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Inventory|Data")
	TObjectPtr<UDataTable> ItemIconTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Inventory|Tabs", meta = (Categories = "Item"))
	FGameplayTag WeaponTabCategoryTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Inventory|Tabs", meta = (Categories = "Item"))
	FGameplayTag ConsumableTabCategoryTag;

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Inventory|Events")
	FRetrieveInventoryWidgetSimpleSignature OnInventoryListChanged;

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Inventory|Events")
	FRetrieveInventorySelectionChangedSignature OnSelectedItemChanged;

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Inventory|Events")
	FRetrieveInventoryWidgetSimpleSignature OnEquipmentChanged;

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	void InitializeInventoryPanel(UInventoryComponent* InInventoryComponent, UWeaponComponent* InWeaponComponent);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	void RefreshInventoryList();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	void SelectItem(FName ItemId, FGameplayTag ItemCategoryTag);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	bool ActivateSelectedItem();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	bool EquipSelectedWeapon();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	bool UnequipCurrentWeapon();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	bool UseSelectedConsumable();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory|QuickSlot")
	void ShowQuickSlotAssignDialog();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory|QuickSlot")
	void HideQuickSlotAssignDialog();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory|QuickSlot")
	bool AssignSelectedConsumableToSlot(int32 SlotKey);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory|QuickSlot")
	bool UnassignSelectedConsumableSlot();

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	TArray<FRetrieveItemStack> GetCurrentItems() const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	FGameplayTag GetCurrentCategoryTag() const { return CurrentCategoryTag; }

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	FName GetSelectedItemId() const { return SelectedItemId; }

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	FGameplayTag GetSelectedItemCategoryTag() const { return SelectedItemCategoryTag; }

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	bool IsSelectedWeaponEquipped() const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	bool IsItemSelected(FName ItemId) const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	bool IsWeaponItemEquipped(FName WeaponItemId) const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	bool CanEquipSelectedWeapon() const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	bool CanUnequipCurrentWeapon() const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	bool CanUseSelectedConsumable() const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory|QuickSlot")
	bool CanAssignSelectedConsumableToQuickSlot() const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory|QuickSlot")
	bool IsSelectedConsumableAssignedToQuickSlot() const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory|QuickSlot")
	int32 GetSelectedConsumableSlotKey() const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory|QuickSlot")
	FName GetQuickSlotItemId(int32 SlotKey) const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory|QuickSlot")
	FText GetQuickSlotDisplayText(int32 SlotKey) const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	bool GetSelectedWeaponData(FRetrieveWeaponDataRow& OutWeaponData) const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	bool GetCurrentWeaponData(FRetrieveWeaponDataRow& OutWeaponData) const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	bool GetSelectedConsumableData(FRetrieveConsumableItemRow& OutConsumableData) const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	bool GetItemIconData(FName ItemId, FRetrieveItemIconRow& OutIconData) const;

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	void RefreshWeaponComparisonText();

	// 인벤토리 내부 탭 전환 (0: 무기, 1: 소모품)
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	void OpenTab(int32 TabIndex);

protected:
	// 라이프사이클
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 이벤트 핸들러
	UFUNCTION()
	void HandleInventoryChanged();

	UFUNCTION()
	void HandleEquippedWeaponChanged(FName WeaponItemId);

	UFUNCTION()
	void HandleConfirmEquipClicked();

	UFUNCTION()
	void HandleCancelEquipClicked();

	UFUNCTION()
	void HandleAssignQuickSlotClicked();

	UFUNCTION()
	void HandleUnassignQuickSlotClicked();

	UFUNCTION()
	void HandleAssignSlot4Clicked();

	UFUNCTION()
	void HandleAssignSlot5Clicked();

	UFUNCTION()
	void HandleCancelQuickSlotAssignClicked();

	UFUNCTION()
	void HandleConsumableSlotChanged(int32 SlotKey, FName ItemId);

	// 컴포넌트 참조
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory")
	TObjectPtr<UInventoryComponent> InventoryComponent;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory")
	TObjectPtr<UWeaponComponent> WeaponComponent;

	// 위젯 바인딩 — UMG 설계 시점에 연결, 런타임 상태와 구분
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_SelectedCompare;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> ScrollBox_ItemList;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UUniformGridPanel> UniformGrid_ItemList;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> HorizontalBox_CurrentWeaponSkillIcons;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> HorizontalBox_SelectedWeaponSkillIcons;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_WeaponSwapConfirm;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_ConfirmEquip;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_CancelEquip;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_QuickSlot4;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_QuickSlot5;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_AssignQuickSlot;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_UnassignQuickSlot;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_QuickSlotAssignDialog;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_AssignSlot4;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_AssignSlot5;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_CancelQuickSlotAssign;

	// 런타임 상태
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory", meta = (Categories = "Item"))
	FGameplayTag CurrentCategoryTag;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory")
	FName SelectedItemId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory", meta = (Categories = "Item"))
	FGameplayTag SelectedItemCategoryTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Inventory")
	int32 ActiveTabIndex = 0;

	// 무기 교체 확인 팝업을 우회할 때만 true. TGuardValue로 사용
	bool bBypassWeaponSwapConfirm = false;
	FVector2D LastInventoryGridAreaSize = FVector2D::ZeroVector;

	// 내부 헬퍼
	void BindInventoryEvents();
	void BindButtonEvents();
	void InitOwnerComponents();
	void InitDefaultTags();
	void RefreshInventoryView(bool bClearSelection);
	void ClearSelection();
	bool IsWeaponCategory(FGameplayTag ItemCategoryTag) const;
	bool IsConsumableCategory(FGameplayTag ItemCategoryTag) const;
	bool ShouldConfirmWeaponSwap() const;
	void ShowWeaponSwapConfirm(bool bShow);
	void UpdateQuickSlotPanel();
	void UpdateQuickSlotActionButtons();
	void RefreshInventoryGridLayout();
	void RefreshWeaponSkillIcons();
	void PopulateWeaponSkillIcons(UHorizontalBox* SkillIconBox, const TArray<FWeaponSkillPreview>& SkillPreviews) const;
	FString BuildWeaponComparisonText() const;
	FString FormatWeaponSummary(const FRetrieveWeaponDataRow& WeaponData) const;
	FString FormatWeaponSkillList(const FRetrieveWeaponDataRow& WeaponData) const;
	static FString GetGameplayTagLeaf(FGameplayTag Tag);
};
