#pragma once

#include "CoreMinimal.h"
#include "Components/PawnComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameplayTagContainer.h"
#include "InventoryComponent.generated.h"

class URetrieveAbilitySystemComponent;
class UWeaponComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FInventoryChangedSignature);
// ItemId + CategoryTag + Quantity — 픽업 토스트 등 UI가 카테고리 구분 없이 DataTable을 찾을 수 있도록 Tag 포함
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FInventoryItemChangedSignature, FName, ItemId, FGameplayTag, ItemCategoryTag, int32, Quantity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEquippedWeaponChangedSignature, FName, WeaponItemId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FConsumableSlotChangedSignature, int32, SlotKey, FName, ItemId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FConsumableSlotUsedSignature, int32, SlotKey);
// 제작 결과 알림 — bSuccess: 성공 여부, RecipeId: 레시피 ID, OutputItemId: 결과물 아이템 ID
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FCraftCompletedSignature, bool, bSuccess, FName, RecipeId, FName, OutputItemId);

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

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	bool RemoveItem(FName ItemId, FGameplayTag ItemCategoryTag, int32 Quantity = 1);

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

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	bool RequestEquipWeapon(FName WeaponItemId);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	bool RequestUnequipWeapon();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	bool UseConsumableItem(FName ConsumableItemId);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	bool AssignConsumableSlot(int32 SlotKey, FName ConsumableItemId);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	bool UnassignConsumableSlot(int32 SlotKey);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	bool UseConsumableSlot(int32 SlotKey);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Inventory")
	bool CraftItem(FName RecipeId);

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
	FConsumableSlotChangedSignature OnConsumableSlotChanged;

	/** 퀵슬롯 키를 눌러 소모품 사용을 시도했을 때 브로드캐스트 (슬롯에 아이템이 있을 때만). UI 피드백용 */
	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Inventory")
	FConsumableSlotUsedSignature OnConsumableSlotUsed;

	/** 제작 완료(성공/실패 모두) 시 브로드캐스트. CraftPanel이 결과를 표시하는 데 사용 */
	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Inventory|Craft")
	FCraftCompletedSignature OnCraftCompleted;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_InventoryItems, Category = "Retrieve|Inventory")
	TArray<FRetrieveItemStack> WeaponItems;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_InventoryItems, Category = "Retrieve|Inventory")
	TArray<FRetrieveItemStack> ConsumableItems;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_InventoryItems, Category = "Retrieve|Inventory")
	TArray<FRetrieveItemStack> MaterialItems;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_EquippedWeaponId, Category = "Retrieve|Inventory")
	FName EquippedWeaponId = NAME_None;

	// 전투 소모품 슬롯 4, 5번. TMap은 복제 불가라 필드로 직접 관리
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_ConsumableSlots, Category = "Retrieve|Inventory")
	FName ConsumableSlot4ItemId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_ConsumableSlots, Category = "Retrieve|Inventory")
	FName ConsumableSlot5ItemId = NAME_None;

	// 픽업 토스트 알림용. 서버의 AddItem 성공 직후 set → REPNOTIFY_Always로 클라이언트에 복제
	// → OnRep_LastAddedItem에서 OnItemAdded.Broadcast — WBP_HUD 토스트 트리거
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_LastAddedItem, Category = "Retrieve|Inventory")
	FRetrieveItemStack LastAddedItemNotification;

	UFUNCTION()
	void OnRep_InventoryItems();

	UFUNCTION()
	void OnRep_EquippedWeaponId();

	UFUNCTION()
	void OnRep_ConsumableSlots();

	UFUNCTION()
	void OnRep_LastAddedItem();

	UFUNCTION(Server, Reliable)
	void ServerRequestEquipWeapon(FName WeaponItemId);

	UFUNCTION(Server, Reliable)
	void ServerRequestUnequipWeapon();

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

	// SlotKey(4 or 5)에 해당하는 필드 참조를 반환
	FName& GetMutableSlotField(int32 SlotKey);
	const FName& GetSlotField(int32 SlotKey) const;

	bool HasAuthorityToModify() const;
	static bool IsValidConsumableSlotKey(int32 SlotKey);
};
