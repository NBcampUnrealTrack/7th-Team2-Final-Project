#pragma once

#include "CoreMinimal.h"
#include "Components/PawnComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameplayTagContainer.h"
#include "InventoryComponent.generated.h"

class URetrieveAbilitySystemComponent;
class UArmorComponent;
class UWeaponComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FInventoryChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCurrencyChangedSignature, int32, NewAmount);
// ItemId + CategoryTag + Quantity — 픽업 토스트 등 UI가 카테고리 구분 없이 DataTable을 찾을 수 있도록 Tag 포함
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FInventoryItemChangedSignature, FName, ItemId, FGameplayTag, ItemCategoryTag, int32, Quantity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEquippedArmorChangedSignature, FGameplayTag, EquipmentSlotTag, FName, ArmorItemId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEquippedWeaponChangedSignature, FName, WeaponItemId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FConsumableSlotChangedSignature, int32, SlotKey, FName, ItemId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FConsumableSlotUsedSignature, int32, SlotKey);
// 제작 결과 알림 — bSuccess: 성공 여부, RecipeId: 레시피 ID, OutputItemId: 결과물 아이템 ID
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FCraftCompletedSignature, bool, bSuccess, FName, RecipeId, FName, OutputItemId);

// 무기+소모품 공용 퀵슬롯 엔트리
USTRUCT(BlueprintType)
struct FRetrieveQuickSlotEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SlotKey = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "Item"))
	FGameplayTag ItemCategoryTag;

	bool IsValid() const
	{
		return SlotKey != INDEX_NONE && !ItemId.IsNone() && ItemCategoryTag.IsValid();
	}

	void Reset()
	{
		SlotKey = INDEX_NONE;
		ItemId = NAME_None;
		ItemCategoryTag = FGameplayTag();
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FRetrieveQuickSlotChangedSignature,
	int32, SlotKey,
	FRetrieveQuickSlotEntry, Entry
);

// 아이템 보유 상태만 관리한다. 전투 반영은 WeaponComponent와 GAS에서 처리
// ItemId는 무기, 소모품, 재료 전체에서 겹치지 않게 사용
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class RETRIEVE_API UInventoryComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	static constexpr int32 QuickSlotPrimaryKey = 4;
	static constexpr int32 QuickSlotSecondaryKey = 5;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	bool AddItem(FName ItemId, FGameplayTag ItemCategoryTag, int32 Quantity = 1);

	// SlotInstanceId(-1=미지정) 지정 시 정확히 그 슬롯 하나만 제거(예: 상점에서 선택한 슬롯 판매).
	// 미지정 시 배열 뒤에서부터 ItemId가 일치하는 슬롯을 필요한 수량만큼 소모(기존 동작).
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	bool RemoveItem(FName ItemId, FGameplayTag ItemCategoryTag, int32 Quantity = 1, int32 SlotInstanceId = -1);

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	bool HasItem(FName ItemId, int32 Quantity = 1) const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	int32 GetItemCount(FName ItemId) const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	TArray<FRetrieveItemStack> GetItemsByCategory(FGameplayTag ItemCategoryTag) const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	bool CanOpenInventory() const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	bool CanChangeEquipment() const;

	// SlotInstanceId: 더블클릭/선택한 정확한 인벤토리 슬롯(FRetrieveItemStack::SlotInstanceId).
	// 같은 무기가 여러 슬롯에 있을 때 어느 슬롯을 장착했는지 UI가 하이라이트할 수 있도록 기록만 한다.
	// 실제 장착 판정(HasItem)은 ItemId 기준이라 생략(INDEX_NONE) 가능.
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	bool RequestEquipWeapon(FName WeaponItemId, int32 SlotInstanceId = -1);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	bool RequestUnequipWeapon();

	// Armor UI 진입점: 위젯은 ArmorComponent를 직접 호출하지 말고 이 함수만 호출한다.
	// 내부에서 보유/상태 검사 후 UArmorComponent::EquipArmor로 위임한다.
	// SlotInstanceId: RequestEquipWeapon과 동일한 목적(정확한 슬롯 하이라이트용).
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	bool RequestEquipArmor(FGameplayTag EquipmentSlotTag, FName ArmorItemId, int32 SlotInstanceId = -1);

	// Armor UI 진입점: 위젯은 ArmorComponent를 직접 호출하지 말고 이 함수만 호출한다.
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	bool RequestUnequipArmor(FGameplayTag EquipmentSlotTag);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	bool UseConsumableItem(FName ConsumableItemId);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	bool AssignConsumableSlot(int32 SlotKey, FName ConsumableItemId);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	bool UnassignConsumableSlot(int32 SlotKey);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	bool UseConsumableSlot(int32 SlotKey);

	// ── 공용 퀵슬롯 (무기 + 소모품) ─────────────────────────────────────────
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory|QuickSlot")
	bool AssignQuickSlotItem(int32 SlotKey, FName ItemId, FGameplayTag ItemCategoryTag);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory|QuickSlot")
	bool UnassignQuickSlotItem(int32 SlotKey);

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory|QuickSlot")
	FRetrieveQuickSlotEntry GetQuickSlotEntry(int32 SlotKey) const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory|QuickSlot")
	int32 GetAssignedQuickSlotKey(FName ItemId, FGameplayTag ItemCategoryTag) const;

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory|QuickSlot")
	bool ActivateQuickSlotItem(int32 SlotKey);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	bool CraftItem(FName RecipeId);

	// ── Currency ──────────────────────────────────────────────────────────────
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory|Currency")
	bool AddCurrency(int32 Amount);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory|Currency")
	bool SpendCurrency(int32 Amount);

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory|Currency")
	int32 GetCurrency() const { return Currency; }

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory|Currency")
	bool HasEnoughCurrency(int32 Amount) const { return Currency >= Amount; }

	/** 제작 가능 여부만 검사 (재료 차감 없음). UI 버튼 활성화 판단에 사용 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	bool CanCraftItem(FName RecipeId) const;

	/** 현재 재료 보유량으로 제작 가능한 최대 횟수. 재료 부족 시 0 반환 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	int32 GetMaxCraftableCount(FName RecipeId) const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory|Craft")
	FText GetCraftRecipeDisplayName(FName RecipeId) const;

	/** 제작 레시피 DataTable. RowName == RecipeId, Row 구조: FRetrieveCraftRecipeRow */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Inventory|Craft")
	TObjectPtr<UDataTable> CraftRecipeTable;

	/** 소모품 효과 DataTable. RowName == ItemId, Row 구조: FRetrieveConsumableItemRow */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Inventory|Consumable")
	TObjectPtr<UDataTable> ConsumableItemTable;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	FName GetEquippedWeaponId() const { return EquippedWeaponId; }

	// 장착된 정확한 인벤토리 슬롯. 같은 무기가 여러 슬롯에 있을 때 UI가 그중 하나만 하이라이트하는 데 사용.
	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	int32 GetEquippedWeaponSlotInstanceId() const { return EquippedWeaponSlotInstanceId; }

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	FName GetEquippedArmorId(FGameplayTag EquipmentSlotTag) const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	TArray<FRetrieveEquippedArmorEntry> GetEquippedArmorSlots() const { return EquippedArmorSlots; }

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	FName GetConsumableSlotItemId(int32 SlotKey) const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory")
	int32 GetAssignedConsumableSlotKey(FName ConsumableItemId) const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Inventory|Save")
	FRetrieveInventorySaveData MakeInventorySaveData() const;

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory|Save")
	bool ApplyInventorySaveData(const FRetrieveInventorySaveData& SaveData, bool bEquipSavedWeapon = true);

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Inventory")
	FInventoryChangedSignature OnInventoryChanged;

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Inventory")
	FInventoryItemChangedSignature OnItemAdded;

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Inventory")
	FInventoryItemChangedSignature OnItemRemoved;

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Inventory")
	FEquippedWeaponChangedSignature OnEquippedWeaponChanged;

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Inventory")
	FEquippedArmorChangedSignature OnEquippedArmorChanged;

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Inventory")
	FConsumableSlotChangedSignature OnConsumableSlotChanged;

	/** 퀵슬롯 키를 눌러 소모품 사용을 시도했을 때 브로드캐스트 (슬롯에 아이템이 있을 때만). UI 피드백용 */
	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Inventory")
	FConsumableSlotUsedSignature OnConsumableSlotUsed;

	/** 무기+소모품 공용 퀵슬롯 변경 이벤트. QuickSlotWheelWidget이 구독 */
	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Inventory|QuickSlot")
	FRetrieveQuickSlotChangedSignature OnQuickSlotChanged;

	/** 제작 완료(성공/실패 모두) 시 브로드캐스트. CraftPanel이 결과를 표시하는 데 사용 */
	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Inventory|Craft")
	FCraftCompletedSignature OnCraftCompleted;

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Inventory|Currency")
	FCurrencyChangedSignature OnCurrencyChanged;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_InventoryItems, Category = "Retrieve|Inventory")
	TArray<FRetrieveItemStack> WeaponItems;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_InventoryItems, Category = "Retrieve|Inventory")
	TArray<FRetrieveItemStack> ConsumableItems;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_InventoryItems, Category = "Retrieve|Inventory")
	TArray<FRetrieveItemStack> MaterialItems;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_InventoryItems, Category = "Retrieve|Inventory")
	TArray<FRetrieveItemStack> ArmorItems;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_EquippedWeaponId, Category = "Retrieve|Inventory")
	FName EquippedWeaponId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_EquippedWeaponId, Category = "Retrieve|Inventory")
	int32 EquippedWeaponSlotInstanceId = INDEX_NONE;

	// 무기/방어구 슬롯에 부여할 다음 고유 ID. 세이브 데이터로 복원되어 이어서 사용된다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Inventory")
	int32 NextSlotInstanceId = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_EquippedArmorSlots, Category = "Retrieve|Inventory")
	TArray<FRetrieveEquippedArmorEntry> EquippedArmorSlots;

	// 공용 퀵슬롯 (비복제, 로컬 전용). 복제가 필요한 슬롯 4&5 소모품은 ConsumableSlot4/5ItemId로 별도 관리
	UPROPERTY()
	TMap<int32, FRetrieveQuickSlotEntry> QuickSlots;

	// 퀵슬롯 연타 시 서버 RPC(RequestEquipWeapon 등)가 중첩 발생해 UI 갱신이 씹히는 것을 막기 위한 입력 쿨다운
	double LastQuickSlotActivateTime = -1.0;

	static constexpr double QuickSlotActivateCooldownSeconds = 0.25;

	// 전투 소모품 슬롯 4, 5번. TMap은 복제 불가라 필드로 직접 관리
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_ConsumableSlots, Category = "Retrieve|Inventory")
	FName ConsumableSlot4ItemId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_ConsumableSlots, Category = "Retrieve|Inventory")
	FName ConsumableSlot5ItemId = NAME_None;

	// 픽업 토스트 알림용. 서버의 AddItem 성공 직후 set → REPNOTIFY_Always로 클라이언트에 복제
	// → OnRep_LastAddedItem에서 OnItemAdded.Broadcast — WBP_HUD 토스트 트리거
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_LastAddedItem, Category = "Retrieve|Inventory")
	FRetrieveItemStack LastAddedItemNotification;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_Currency, Category = "Retrieve|Inventory|Currency")
	int32 Currency = 0;

	UFUNCTION()
	void OnRep_InventoryItems();

	UFUNCTION()
	void OnRep_EquippedWeaponId();

	UFUNCTION()
	void OnRep_EquippedArmorSlots();

	UFUNCTION()
	void OnRep_ConsumableSlots();

	UFUNCTION()
	void OnRep_LastAddedItem();

	UFUNCTION()
	void OnRep_Currency();

	UFUNCTION(Server, Reliable)
	void ServerRequestEquipWeapon(FName WeaponItemId, int32 SlotInstanceId);

	UFUNCTION(Server, Reliable)
	void ServerRequestUnequipWeapon();

	UFUNCTION(Server, Reliable)
	void ServerRequestEquipArmor(FGameplayTag EquipmentSlotTag, FName ArmorItemId, int32 SlotInstanceId);

	UFUNCTION(Server, Reliable)
	void ServerRequestUnequipArmor(FGameplayTag EquipmentSlotTag);

	UFUNCTION(Server, Reliable)
	void ServerUseConsumableItem(FName ConsumableItemId);

	UFUNCTION(Server, Reliable)
	void ServerAssignConsumableSlot(int32 SlotKey, FName ConsumableItemId);

	UFUNCTION(Server, Reliable)
	void ServerUnassignConsumableSlot(int32 SlotKey);

	UFUNCTION(Server, Reliable)
	void ServerUseConsumableSlot(int32 SlotKey);

	UFUNCTION(Server, Reliable)
	void ServerCraftItem(FName RecipeId);

	UFUNCTION(Server, Reliable)
	void ServerAssignQuickSlotItem(int32 SlotKey, FName ItemId, FGameplayTag ItemCategoryTag);

	UFUNCTION(Server, Reliable)
	void ServerUnassignQuickSlotItem(int32 SlotKey);

	UFUNCTION(Server, Reliable)
	void ServerAddCurrency(int32 Amount);

	UFUNCTION(Server, Reliable)
	void ServerSpendCurrency(int32 Amount);

	UArmorComponent* GetArmorComponent() const;
	UWeaponComponent* GetWeaponComponent() const;
	URetrieveAbilitySystemComponent* GetRetrieveAbilitySystemComponent() const;
	const FRetrieveConsumableItemRow* FindConsumableItemRow(FName ConsumableItemId) const;
	UDataTable* ResolveCraftRecipeTable() const;
	const FRetrieveCraftRecipeRow* FindCraftRecipeRow(FName RecipeId, const TCHAR* Context) const;
	bool IsCraftRecipeValid(const FRetrieveCraftRecipeRow& Recipe) const;
	bool HasRequiredCraftMaterials(const FRetrieveCraftRecipeRow& Recipe) const;
	bool ConsumeCraftMaterials(const FRetrieveCraftRecipeRow& Recipe);
	bool ApplyConsumableEffects(const FRetrieveConsumableItemRow& ConsumableRow);

	// const 버전 하나만 구현, mutable 버전은 const_cast로 위임
	const TArray<FRetrieveItemStack>* GetItemsForCategory(FGameplayTag ItemCategoryTag) const;
	TArray<FRetrieveItemStack>* GetMutableItemsForCategory(FGameplayTag ItemCategoryTag);

	const FRetrieveItemStack* FindStack(FName ItemId) const;
	FRetrieveItemStack* FindMutableStack(FName ItemId);

	const FRetrieveEquippedArmorEntry* FindEquippedArmorSlot(FGameplayTag EquipmentSlotTag) const;
	FRetrieveEquippedArmorEntry* FindMutableEquippedArmorSlot(FGameplayTag EquipmentSlotTag);
	void RemoveEquippedArmorSlot(FGameplayTag EquipmentSlotTag);

	// SlotKey(4 or 5)에 해당하는 필드 참조를 반환
	FName& GetMutableSlotField(int32 SlotKey);
	const FName& GetSlotField(int32 SlotKey) const;

	bool HasAuthorityToModify() const;
	static bool IsValidConsumableSlotKey(int32 SlotKey);
};
