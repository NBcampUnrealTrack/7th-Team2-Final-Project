#pragma once

#include "CoreMinimal.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameplayTagContainer.h"
#include "UI/RetrieveGamePanelWidget.h"
#include "InventoryPanelWidget.generated.h"

class UInventoryComponent;
class URetrieveAbilitySystemComponent;
class UWeaponComponent;
class URetrieveQuickSlotWheelWidget;
class UBorder;
class UButton;
class UHorizontalBox;
class UImage;
class UScrollBox;
class UTextBlock;
class UUniformGridPanel;
class UUserWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRetrieveInventoryWidgetSimpleSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FRetrieveInventorySelectionChangedSignature, FName, ItemId, FGameplayTag, ItemCategoryTag);

/** 인벤토리 아이템 정렬 기준 */
UENUM(BlueprintType)
enum class EInventorySortMode : uint8
{
	None            UMETA(DisplayName = "기본 (획득순)"),
	NameAsc         UMETA(DisplayName = "이름 오름차순"),
	NameDesc        UMETA(DisplayName = "이름 내림차순"),
	TypeAsc         UMETA(DisplayName = "타입 오름차순"),
	TypeDesc        UMETA(DisplayName = "타입 내림차순"),
	AttackPowerAsc  UMETA(DisplayName = "공격력 오름차순"),  // 무기 탭 전용
	AttackPowerDesc UMETA(DisplayName = "공격력 내림차순"), // 무기 탭 전용
	DefenseAsc      UMETA(DisplayName = "방어력 오름차순"),  // 방어구 탭 전용
	DefenseDesc     UMETA(DisplayName = "방어력 내림차순"), // 방어구 탭 전용
};

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Inventory|Data")
	TObjectPtr<UDataTable> MaterialItemTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Inventory|Tooltip")
	TSubclassOf<UUserWidget> ItemDetailTooltipWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Inventory|Tooltip")
	TSubclassOf<UUserWidget> ItemCompareTooltipWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Inventory|Tabs", meta = (Categories = "Item"))
	FGameplayTag WeaponTabCategoryTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Inventory|Tabs", meta = (Categories = "Item"))
	FGameplayTag ConsumableTabCategoryTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Inventory|Tabs", meta = (Categories = "Item"))
	FGameplayTag MaterialTabCategoryTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Inventory|Data")
	TObjectPtr<UDataTable> ArmorDataTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Inventory|Tabs", meta = (Categories = "Item"))
	FGameplayTag ArmorTabCategoryTag;

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Inventory|Events")
	FRetrieveInventoryWidgetSimpleSignature OnInventoryListChanged;

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Inventory|Events")
	FRetrieveInventorySelectionChangedSignature OnSelectedItemChanged;

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Inventory|Events")
	FRetrieveInventoryWidgetSimpleSignature OnEquipmentChanged;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Inventory|Input", meta = (ClampMin = "0.0"))
	float SelectedItemActivationGuardSeconds = 0.25f;

	/** 그리드 슬롯에서 같은 아이템을 연속 클릭했을 때, 이 시간 안에 들어와야만 "빠른 더블클릭"으로 인정해 장착/해제를 실행한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Inventory|Input", meta = (ClampMin = "0.0"))
	float FastDoubleClickThresholdSeconds = 0.35f;

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	void InitializeInventoryPanel(UInventoryComponent* InInventoryComponent, UWeaponComponent* InWeaponComponent);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	void RefreshInventoryList();

	// SlotInstanceId: 클릭한 정확한 슬롯(FRetrieveItemStack::SlotInstanceId). 그리드 슬롯 위젯이 자신의 아이템 스택에서
	// 값을 그대로 전달해야 한다 — 생략(INDEX_NONE) 시 장착해도 어느 슬롯인지 하이라이트할 수 없다.
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	void SelectItem(FName ItemId, FGameplayTag ItemCategoryTag, int32 SlotInstanceId = -1);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	bool ActivateSelectedItem();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	bool SelectAndActivateItem(FName ItemId, FGameplayTag ItemCategoryTag, int32 SlotInstanceId = -1);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	bool EquipSelectedWeapon();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	bool UnequipCurrentWeapon();

	/** 무기 해제 성공 시 BP가 프리뷰 액터의 PreviewWeapon(None)을 호출하도록 알린다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|Inventory")
	void OnWeaponPreviewClearNeeded();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	bool EquipSelectedArmor();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	bool UnequipSelectedArmor();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	bool UseSelectedConsumable();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory|QuickSlot")
	void ShowQuickSlotAssignDialog();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory|QuickSlot")
	void HideQuickSlotAssignDialog();

	// ── 공용 퀵슬롯 (무기 + 소모품) ─────────────────────────────────────────
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory|QuickSlot")
	bool AssignSelectedItemToQuickSlot(int32 SlotKey, bool bForceReplace = false);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory|QuickSlot")
	bool UnassignSelectedQuickSlot();

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory|QuickSlot")
	bool CanAssignSelectedItemToQuickSlot() const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory|QuickSlot")
	bool IsSelectedItemAssignedToQuickSlot() const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory|QuickSlot")
	int32 GetSelectedQuickSlotKey() const;

	// ── 기존 소모품 전용 (하위 호환 래퍼) ─────────────────────────────────
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory|QuickSlot")
	bool AssignSelectedConsumableToSlot(int32 SlotKey);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory|QuickSlot")
	bool UnassignSelectedConsumableSlot();

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	TArray<FRetrieveItemStack> GetCurrentItems() const;

	// ── 정렬 ──────────────────────────────────────────────────────────────
	/** 정렬 모드를 직접 지정하고 목록을 갱신한다 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory|Sort")
	void SetSortMode(EInventorySortMode NewMode);

	/** 이름순 정렬을 토글한다 (None → NameAsc → NameDesc → None) */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory|Sort")
	void CycleSortByName();

	/** 타입순 정렬을 토글한다 (None → TypeAsc → TypeDesc → None) */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory|Sort")
	void CycleSortByType();

	/** 공격력순 정렬을 토글한다 — 무기 탭에서만 의미 있음 (None → AtkAsc → AtkDesc → None) */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory|Sort")
	void CycleSortByAttackPower();

	/** 현재 정렬 모드 조회 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory|Sort")
	EInventorySortMode GetCurrentSortMode() const { return CurrentSortMode; }

	/** 현재 탭 아이템을 CurrentSortMode에 따라 정렬해 반환 (BP 목록 갱신 시 GetCurrentItems 대신 사용) */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory|Sort")
	TArray<FRetrieveItemStack> GetCurrentItemsSorted() const;

	/** Button_SortAttackPower VisibilityDelegate 바인딩용 — 무기 탭일 때만 Visible */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory|Sort")
	ESlateVisibility GetSortAttackPowerButtonVisibility() const;

	/** 방어력순 정렬을 토글한다 (None → DefenseAsc → DefenseDesc → None) */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory|Sort")
	void CycleSortByDefense();

	/** Button_SortDefense VisibilityDelegate 바인딩용 — 방어구 탭일 때만 Visible */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory|Sort")
	ESlateVisibility GetSortDefenseButtonVisibility() const;

	// ── 최종 스탯 조회 ────────────────────────────────────────────────────
	/** 캐릭터 기본 공격력 (무기 보정 전 Base 값) */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory|Stats")
	float GetCharacterBaseAttackPower() const;

	/** 현재 장착된 무기의 공격력 보너스 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory|Stats")
	float GetWeaponBonusAttackPower() const;

	/** ASC 어트리뷰트 기준 최종 공격력 (Base + 무기 GE 포함 모든 Modifier 적용 후) */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory|Stats")
	float GetTotalAttackPower() const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory|Stats")
	float GetCharacterBaseDefense() const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory|Stats")
	float GetArmorBonusDefense() const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory|Stats")
	float GetTotalDefense() const;

	/** ASC 어트리뷰트 기준 최대 체력 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory|Stats")
	float GetTotalMaxHealth() const;

	/** ASC 어트리뷰트 기준 이동 속도 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory|Stats")
	float GetTotalMoveSpeed() const;

	/** 인벤토리 스탯 패널용 포맷 텍스트: "기본 ATK: N\n무기 보너스: +M\n최종 ATK: P" */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory|Stats")
	FText GetFinalStatDisplayText() const;

	/**
	 * HP + ATK 전체 스탯 표시 텍스트.
	 * DT_CharacterStats에 새 컬럼(float/int) 추가 시 자동으로 출력에 포함됨.
	 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory|Stats")
	FText GetFullStatDisplayText() const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	FGameplayTag GetCurrentCategoryTag() const { return CurrentCategoryTag; }

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	FName GetSelectedItemId() const { return SelectedItemId; }

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	FGameplayTag GetSelectedItemCategoryTag() const { return SelectedItemCategoryTag; }

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	bool IsSelectedWeaponEquipped() const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	bool IsItemSelected(FName ItemId, int32 SlotInstanceId = -1) const;

	// 장비는 ItemId와 SlotInstanceId가 모두 일치할 때만 장착 슬롯으로 판정한다.
	// 시작 장비처럼 SlotInstanceId가 INDEX_NONE인 스택도 장착 측 값 역시 INDEX_NONE이면 일치로 처리한다.
	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	bool IsWeaponItemEquipped(FName WeaponItemId, int32 SlotInstanceId = -1) const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	bool IsArmorItemEquipped(FName ArmorItemId, int32 SlotInstanceId = -1) const;

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

	// QuickSlotWheel 클래스 — 에디터에서 WBP_QuickSlotWheel 지정
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Inventory|QuickSlot")
	TSubclassOf<URetrieveQuickSlotWheelWidget> QuickSlotWheelClass;

	UPROPERTY()
	TObjectPtr<URetrieveQuickSlotWheelWidget> QuickSlotWheelInstance;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory|QuickSlot")
	FText GetQuickSlotDisplayText(int32 SlotKey) const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	bool GetSelectedWeaponData(FRetrieveWeaponDataRow& OutWeaponData) const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	bool GetCurrentWeaponData(FRetrieveWeaponDataRow& OutWeaponData) const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	bool GetSelectedConsumableData(FRetrieveConsumableItemRow& OutConsumableData) const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	bool GetSelectedMaterialData(FRetrieveMaterialItemRow& OutMaterialData) const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	bool GetSelectedArmorData(FRetrieveArmorDataRow& OutArmorData) const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	bool IsSelectedArmorEquipped() const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	bool CanEquipSelectedArmor() const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	bool CanUnequipSelectedArmor() const;

	/** 탭에 무관하게 현재 선택 아이템을 장착할 수 있으면 true — Button_Equip 가시성 바인딩용 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	bool CanEquipSelected() const;

	/** 탭에 무관하게 현재 선택/장착 상태를 해제할 수 있으면 true — Button_Unequip 가시성 바인딩용 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	bool CanUnequipSelected() const;

	/** 프리뷰 슬롯 클릭 시 해당 슬롯의 장착 아이템을 인벤토리에서 선택 상태로 만든다 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	void SelectEquipmentSlot(FGameplayTag EquipmentSlotTag);

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	FName GetEquippedArmorIdBySlot(FGameplayTag EquipmentSlotTag) const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	TArray<FRetrieveEquippedArmorEntry> GetAllEquippedArmorSlots() const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	bool GetItemIconData(FName ItemId, FRetrieveItemIconRow& OutIconData) const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory|Tooltip")
	FText BuildItemTooltipText(FName ItemId, FGameplayTag ItemCategoryTag) const;

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	void RefreshWeaponComparisonText();

	/**
	 * 현재 선택된 아이템의 상태 텍스트를 반환한다.
	 *   무기:    "장착 중" / "보관 중"
	 *   소모품:  "퀵슬롯 4" / "퀵슬롯 5" / "없음"
	 *   재료:    "보유 N개"
	 * BP에서 Text_DetailState 바인딩 또는 OnSelectedItemChanged 핸들러에서 사용.
	 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	FText GetSelectedItemStateText() const;

	// 인벤토리 내부 탭 전환 (0: 무기, 1: 소모품, 2: 재료)
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	void OpenTab(int32 TabIndex);

protected:
	// 라이프사이클
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 이벤트 핸들러
	UFUNCTION()
	void HandleInventoryChanged();

	UFUNCTION()
	void HandleCurrencyChanged(int32 NewAmount);

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

	UFUNCTION()
	void HandleQuickSlotWheelSlotClicked(int32 SlotKey);

	UFUNCTION()
	void ConfirmQuickSlotReplace();

	UFUNCTION()
	void CancelQuickSlotReplace();

	UFUNCTION()
	void HandleMaterialTabClicked();

	UFUNCTION()
	void HandleArmorTabClicked();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	void HandleEquipClicked();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	void HandleUnequipClicked();

	UFUNCTION()
	void HandleEquippedArmorChanged(FGameplayTag EquipmentSlotTag, FName ArmorItemId);

	void RefreshSlotButtonTooltips();
	void RefreshSlotIcons();
	void RefreshStatDisplay();

	UFUNCTION()
	void HandleSlotHeadClicked();
	UFUNCTION()
	void HandleSlotWeaponClicked();
	UFUNCTION()
	void HandleSlotChestClicked();
	UFUNCTION()
	void HandleSlotHandsLClicked();
	UFUNCTION()
	void HandleSlotHandsRClicked();
	UFUNCTION()
	void HandleSlotLegsClicked();
	UFUNCTION()
	void HandleSlotFeetClicked();

	// 컴포넌트 참조
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory")
	TObjectPtr<UInventoryComponent> InventoryComponent;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory")
	TObjectPtr<UWeaponComponent> WeaponComponent;

	// 위젯 바인딩 — UMG 설계 시점에 연결, 런타임 상태와 구분
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CurrencyDisplay;

	// 캐릭터 스탯 표시 (Monolith GAS 어트리뷰트 바인딩은 Editor 전용 모듈이라 Shipping에서
	// 동작하지 않으므로 네이티브 코드로 대체 — RefreshStatDisplay() 참고).
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_StatAtkValue;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_StatDefValue;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_StatHpValue;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_StatSpdValue;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_SelectedCompare;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_FinalStatDisplay;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_DetailType;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_DetailState;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_DetailName;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_DetailMainStat;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_DetailElement;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_DetailDescription;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> ScrollBox_ItemList;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UUniformGridPanel> UniformGrid_ItemList;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> HorizontalBox_Tabs;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_TabMaterial;

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

	// 교체 확인 다이얼로그 — UMG에 추가 후 BindWidgetOptional로 연결
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_QuickSlotReplaceConfirm;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_ConfirmQuickSlotReplace;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_CancelQuickSlotReplace;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_QuickSlotReplaceMessage;

	/** 무기 탭일 때만 표시되는 공격력 정렬 버튼 */
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_SortAttackPower;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_TabArmor;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Equip;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Unequip;

	// 선택 아이템이 소모품일 때만 표시 (가시성은 UpdateEquipActionButtons에서 제어)
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_UseConsumable;

	/** 방어구 탭일 때만 표시되는 방어력 정렬 버튼 */
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_SortDefense;

	// 장착 프리뷰 오버레이 슬롯 버튼
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_SlotHead;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_SlotWeapon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_SlotChest;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_SlotHands_L;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_SlotHands_R;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_SlotLegs;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_SlotFeet;

	// 장착 프리뷰 슬롯 아이콘 이미지
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_SlotIcon_Head;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_SlotIcon_Weapon;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_SlotIcon_Chest;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_SlotIcon_Hands_L;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_SlotIcon_Hands_R;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_SlotIcon_Legs;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_SlotIcon_Feet;

	// 런타임 상태
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory", meta = (Categories = "Item"))
	FGameplayTag CurrentCategoryTag;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory")
	FName SelectedItemId = NAME_None;

	// 같은 무기/방어구가 여러 슬롯에 있을 때 정확히 어느 슬롯을 선택/더블클릭했는지 기록 (FRetrieveItemStack::SlotInstanceId).
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory")
	int32 SelectedSlotInstanceId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory", meta = (Categories = "Item"))
	FGameplayTag SelectedItemCategoryTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Inventory")
	int32 ActiveTabIndex = 0;

	// 현재 정렬 모드 (무기/방어구 탭은 각 전투 스탯 내림차순이 기본값)
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory|Sort")
	EInventorySortMode CurrentSortMode = EInventorySortMode::AttackPowerDesc;

	// 교체 확인 대기 상태
	int32 PendingReplaceSlotKey = INDEX_NONE;
	FName PendingQuickSlotItemId = NAME_None;
	FGameplayTag PendingQuickSlotCategoryTag;

	// 교체 확인 창을 앞에 보이게 하려고 휠을 패널 아래로 내렸는지 여부 (복원용)
	bool bQuickSlotWheelLowered = false;

	// 무기 교체 확인 팝업을 우회할 때만 true. TGuardValue로 사용
	bool bBypassWeaponSwapConfirm = false;
	bool bBypassSelectedItemActivationGuard = false;
	FVector2D LastInventoryGridAreaSize = FVector2D::ZeroVector;

	// 내부 헬퍼
	void BindInventoryEvents();
	void BindButtonEvents();
	void InitOwnerComponents();
	void InitDefaultTags();
	void ResolveDefaultTooltipWidgetClasses();
	void RefreshInventoryView(bool bClearSelection);
	void ClearSelection();
	bool IsWeaponCategory(FGameplayTag ItemCategoryTag) const;
	bool IsConsumableCategory(FGameplayTag ItemCategoryTag) const;
	bool IsMaterialCategory(FGameplayTag ItemCategoryTag) const;
	bool IsArmorCategory(FGameplayTag ItemCategoryTag) const;
	void RefreshSelectedArmorDetails();
	void UpdateEquipActionButtons();
	// 장착/해제 버튼의 '표시 여부'는 장비 변경 잠금(CanChangeEquipment)과 무관하게 대상 존재 여부로 결정.
	// 잠금은 버튼 활성화(SetIsEnabled)로만 반영한다.
	bool ShouldShowEquipButton() const;
	bool ShouldShowUnequipButton() const;
	// 전투/회피 등 장비 변경 잠금 태그가 바뀌면 버튼 활성 상태를 재평가하기 위한 ASC 태그 이벤트 구독
	void BindEquipLockTagEvents();
	void UnbindEquipLockTagEvents();
	void HandleEquipLockTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
	float GetItemDefensePower(const FRetrieveItemStack& Item) const;
	bool ShouldConfirmWeaponSwap() const;
	void ShowWeaponSwapConfirm(bool bShow);
	void UpdateQuickSlotPanel();
	void UpdateQuickSlotActionButtons();
	UDataTable* ResolveMaterialItemTable() const;
	void RefreshSelectedMaterialDetails();
	// Text_DetailState를 현재 선택 아이템 기준으로 즉시 갱신
	void RefreshSelectedDetailState();
	void RefreshInventoryGridLayout();
	void OpenQuickSlotWheelForAssign();
	void ShowQuickSlotReplaceConfirm(bool bShow);
	// 교체 확인 다이얼로그를 마우스 커서 옆으로 이동
	void PositionConfirmDialogNearCursor(UWidget* DialogWidget) const;
	// 교체 확인용으로 패널 아래로 내렸던 퀵슬롯 휠을 원래 z-order/조작 가능 상태로 복원
	void RestoreQuickSlotWheelZOrder();
	void MarkInventoryTooltipsDirty();
	void ApplyInventorySlotTooltips();
	void ClearWidgetTooltipRecursive(UWidget* Widget) const;
	bool ShouldUseCompareTooltipForItem(const FRetrieveItemStack& Item) const;
	// 비교 툴팁이 "현재 기준"으로 삼는 장착 아이템 ID (무기: 장착 무기, 방어구: 해당 슬롯 장착 방어구)
	FName GetCompareReferenceItemId(const FRetrieveItemStack& Item) const;
	UWidget* CreateInventorySlotTooltip(const FRetrieveItemStack& Item);
	void PopulateFantasyTooltipWidget(
		UUserWidget* TooltipWidget,
		const FRetrieveItemStack& Item,
		const FString& BadgeText,
		const FString& OverrideMainStat = FString(),
		const FString& OverrideRarity = FString(),
		int32 OverrideBasePrice = -1) const;
	UWidget* BuildEquipmentSlotTooltipWidget(const FRetrieveItemStack& Item) const;
	UWidget* CreateInventoryCompareTooltip(
		UUserWidget* TooltipWidget,
		const FRetrieveItemStack& HoveredItem,
		const FRetrieveWeaponDataRow& CurrentWeapon,
		const FRetrieveWeaponDataRow& HoveredWeapon);
	UWidget* CreateInventoryCompareTooltip(
		UUserWidget* TooltipWidget,
		const FRetrieveItemStack& HoveredItem,
		const FRetrieveArmorDataRow& CurrentArmor,
		const FRetrieveArmorDataRow& HoveredArmor);
	void AddTooltipTextLine(class UVerticalBox* LineBox, const FString& Line, const FLinearColor& Color, bool bHeading) const;
	void InvokeTooltipTextFunction(UUserWidget* TooltipWidget, FName FunctionName, const TArray<FString>& Values) const;
	FString FormatWeaponTooltipBlock(const FRetrieveWeaponDataRow& WeaponData, const FString& Header) const;
	FString BuildWeaponSwapDeltaText(const FRetrieveWeaponDataRow& CurrentWeapon, const FRetrieveWeaponDataRow& HoveredWeapon) const;
	FString FormatArmorTooltipBlock(const FRetrieveArmorDataRow& ArmorData, const FString& Header) const;
	FString BuildArmorSwapDeltaText(const FRetrieveArmorDataRow& CurrentArmor, const FRetrieveArmorDataRow& HoveredArmor) const;
	void DisableLegacyTooltipRecursive(UWidget* Widget) const;
	void SuppressBlueprintManagedTooltip();
	bool IsWidgetOrDescendantHovered(const UWidget* Widget) const;
	void RefreshWeaponSkillIcons();
	void PopulateWeaponSkillIcons(UHorizontalBox* SkillIconBox, const TArray<FWeaponSkillPreview>& SkillPreviews) const;
	FString BuildWeaponComparisonText() const;
	FString FormatWeaponSummary(const FRetrieveWeaponDataRow& WeaponData) const;
	FString FormatWeaponSkillList(const FRetrieveWeaponDataRow& WeaponData) const;
	bool CanProcessSelectedItemActivation();
	static FString GetGameplayTagLeaf(FGameplayTag Tag);

	// 정렬 헬퍼
	void SortItemStacks(TArray<FRetrieveItemStack>& Items) const;
	FString GetItemDisplayName(const FRetrieveItemStack& Item) const;
	FString GetItemTypeName(const FRetrieveItemStack& Item) const;
	float GetItemAttackPower(const FRetrieveItemStack& Item) const;

	// 스탯 조회 헬퍼
	TArray<FName> AppliedTooltipItemIds;
	TArray<FGameplayTag> AppliedTooltipCategoryTags;
	TArray<bool> AppliedTooltipCompareFlags;
	TArray<FName> AppliedTooltipCompareReferenceIds;
	bool bInventoryTooltipsDirty = true;
	FName AppliedTooltipEquippedWeaponId = NAME_None;
	double LastSelectedItemActivationTime = -1.0;
	FName LastActivatedItemId = NAME_None;
	FGameplayTag LastActivatedItemCategoryTag;
	int32 LastActivatedSlotInstanceId = INDEX_NONE;

	// SelectAndActivateItem의 빠른 더블클릭 판정 기준 시각 (-1.0이면 대기 중인 클릭 없음)
	double LastGridSlotClickTime = -1.0;

	// 장비 변경 잠금 태그 이벤트 구독 대상 ASC와 핸들 (재구독/해제용)
	TWeakObjectPtr<URetrieveAbilitySystemComponent> BoundEquipLockASC;
	TArray<FDelegateHandle> EquipLockTagHandles;

	URetrieveAbilitySystemComponent* GetOwnerASC() const;
};
