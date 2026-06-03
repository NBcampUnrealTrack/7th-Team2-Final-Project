#pragma once

#include "CoreMinimal.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameplayTagContainer.h"
#include "UI/RetrieveGamePanelWidget.h"
#include "InventoryPanelWidget.generated.h"

class UInventoryComponent;
class URetrieveAbilitySystemComponent;
class UWeaponComponent;
class UBorder;
class UButton;
class UHorizontalBox;
class UScrollBox;
class UTextBlock;
class UUniformGridPanel;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Inventory|Tabs", meta = (Categories = "Item"))
	FGameplayTag WeaponTabCategoryTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Inventory|Tabs", meta = (Categories = "Item"))
	FGameplayTag ConsumableTabCategoryTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Inventory|Tabs", meta = (Categories = "Item"))
	FGameplayTag MaterialTabCategoryTag;

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
	bool GetSelectedMaterialData(FRetrieveMaterialItemRow& OutMaterialData) const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	bool GetItemIconData(FName ItemId, FRetrieveItemIconRow& OutIconData) const;

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	void RefreshWeaponComparisonText();

	// 인벤토리 내부 탭 전환 (0: 무기, 1: 소모품, 2: 재료)
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

	UFUNCTION()
	void HandleMaterialTabClicked();

	// 컴포넌트 참조
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory")
	TObjectPtr<UInventoryComponent> InventoryComponent;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory")
	TObjectPtr<UWeaponComponent> WeaponComponent;

	// 위젯 바인딩 — UMG 설계 시점에 연결, 런타임 상태와 구분
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_SelectedCompare;

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

	/** 무기 탭일 때만 표시되는 공격력 정렬 버튼 */
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_SortAttackPower;

	// 런타임 상태
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory", meta = (Categories = "Item"))
	FGameplayTag CurrentCategoryTag;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory")
	FName SelectedItemId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory", meta = (Categories = "Item"))
	FGameplayTag SelectedItemCategoryTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Inventory")
	int32 ActiveTabIndex = 0;

	// 현재 정렬 모드 (탭 전환 시 None으로 리셋됨)
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Inventory|Sort")
	EInventorySortMode CurrentSortMode = EInventorySortMode::None;

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
	bool IsMaterialCategory(FGameplayTag ItemCategoryTag) const;
	bool ShouldConfirmWeaponSwap() const;
	void ShowWeaponSwapConfirm(bool bShow);
	void UpdateQuickSlotPanel();
	void UpdateQuickSlotActionButtons();
	UDataTable* ResolveMaterialItemTable() const;
	void RefreshSelectedMaterialDetails();
	void RefreshInventoryGridLayout();
	void RefreshWeaponSkillIcons();
	void PopulateWeaponSkillIcons(UHorizontalBox* SkillIconBox, const TArray<FWeaponSkillPreview>& SkillPreviews) const;
	FString BuildWeaponComparisonText() const;
	FString FormatWeaponSummary(const FRetrieveWeaponDataRow& WeaponData) const;
	FString FormatWeaponSkillList(const FRetrieveWeaponDataRow& WeaponData) const;
	static FString GetGameplayTagLeaf(FGameplayTag Tag);

	// 정렬 헬퍼
	void SortItemStacks(TArray<FRetrieveItemStack>& Items) const;
	FString GetItemDisplayName(const FRetrieveItemStack& Item) const;
	FString GetItemTypeName(const FRetrieveItemStack& Item) const;
	float GetItemAttackPower(const FRetrieveItemStack& Item) const;

	// 스탯 조회 헬퍼
	URetrieveAbilitySystemComponent* GetOwnerASC() const;
};
