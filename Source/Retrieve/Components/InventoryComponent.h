#pragma once

#include "CoreMinimal.h"
#include "Components/PawnComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameplayTagContainer.h"
#include "InventoryComponent.generated.h"

class URetrieveAbilitySystemComponent;
class UWeaponComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FInventoryChangedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FInventoryItemChangedSignature, FName, ItemId, int32, Quantity);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEquippedWeaponChangedSignature, FName, WeaponItemId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FConsumableSlotChangedSignature, int32, SlotKey, FName, ItemId);

// 아이템 보유 상태만 관리한다. 전투 반영은 WeaponComponent와 GAS에서 처리
// ItemId는 무기, 소모품, 재료 전체에서 겹치지 않게 사용
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class RETRIEVE_API UInventoryComponent : public UPawnComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

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

	UFUNCTION()
	void OnRep_InventoryItems();

	UFUNCTION()
	void OnRep_EquippedWeaponId();

	UFUNCTION()
	void OnRep_ConsumableSlots();

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

	UWeaponComponent* GetWeaponComponent() const;
	URetrieveAbilitySystemComponent* GetRetrieveAbilitySystemComponent() const;

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
