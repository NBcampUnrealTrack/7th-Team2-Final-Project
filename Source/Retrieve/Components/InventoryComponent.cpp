#include "Components/InventoryComponent.h"

#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Components/RetrievePawnExtensionComponent.h"
#include "Components/WeaponComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Net/UnrealNetwork.h"

UInventoryComponent::UInventoryComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 인벤토리 데이터는 소유 클라이언트만 수신. 다른 플레이어의 인벤토리는 볼 필요 없음
	DOREPLIFETIME_CONDITION(UInventoryComponent, WeaponItems,          COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UInventoryComponent, ConsumableItems,       COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UInventoryComponent, MaterialItems,         COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UInventoryComponent, EquippedWeaponId,      COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UInventoryComponent, ConsumableSlot4ItemId, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UInventoryComponent, ConsumableSlot5ItemId, COND_OwnerOnly);
}

bool UInventoryComponent::AddItem(FName ItemId, FGameplayTag ItemCategoryTag, int32 Quantity)
{
	if (!HasAuthorityToModify())
	{
		UE_LOG(LogTemp, Warning, TEXT("인벤토리 아이템 추가 실패: 서버 권한 없음 ItemId=%s Tag=%s Quantity=%d"),
			*ItemId.ToString(), *ItemCategoryTag.ToString(), Quantity);
		return false;
	}

	if (ItemId.IsNone() || !ItemCategoryTag.IsValid() || Quantity <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("인벤토리 아이템 추가 실패: 입력값이 올바르지 않음 ItemId=%s Tag=%s Quantity=%d"),
			*ItemId.ToString(), *ItemCategoryTag.ToString(), Quantity);
		return false;
	}

	TArray<FRetrieveItemStack>* Items = GetMutableItemsForCategory(ItemCategoryTag);
	if (!Items)
	{
		UE_LOG(LogTemp, Warning, TEXT("인벤토리 아이템 추가 실패: 지원하지 않는 카테고리 Tag=%s ItemId=%s"),
			*ItemCategoryTag.ToString(), *ItemId.ToString());
		return false;
	}

	if (ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Weapon))
	{
		// 무기는 같은 ItemId 중복 적재 없이 성공 처리
		if (FindStack(ItemId))
		{
			return true;
		}

		FRetrieveItemStack& NewStack = Items->AddDefaulted_GetRef();
		NewStack.ItemId = ItemId;
		NewStack.ItemCategoryTag = ItemCategoryTag;
		NewStack.Quantity = 1;
		UE_LOG(LogTemp, Log, TEXT("인벤토리 무기 추가: ItemId=%s Quantity=1"), *ItemId.ToString());
		OnItemAdded.Broadcast(ItemId, 1);
		OnInventoryChanged.Broadcast();
		return true;
	}

	// 소모품, 재료 — 기존 스택에 수량 합산, 없으면 새 스택 생성
	if (FRetrieveItemStack* ExistingStack = FindMutableStack(ItemId))
	{
		ExistingStack->Quantity += Quantity;
		UE_LOG(LogTemp, Log, TEXT("인벤토리 아이템 수량 증가: ItemId=%s Added=%d Total=%d"),
			*ItemId.ToString(), Quantity, ExistingStack->Quantity);
	}
	else
	{
		FRetrieveItemStack& NewStack = Items->AddDefaulted_GetRef();
		NewStack.ItemId = ItemId;
		NewStack.ItemCategoryTag = ItemCategoryTag;
		NewStack.Quantity = Quantity;
		UE_LOG(LogTemp, Log, TEXT("인벤토리 아이템 추가: ItemId=%s Tag=%s Quantity=%d"),
			*ItemId.ToString(), *ItemCategoryTag.ToString(), Quantity);
	}

	OnItemAdded.Broadcast(ItemId, Quantity);
	OnInventoryChanged.Broadcast();
	return true;
}

bool UInventoryComponent::RemoveItem(FName ItemId, FGameplayTag ItemCategoryTag, int32 Quantity)
{
	if (!HasAuthorityToModify() || ItemId.IsNone() || !ItemCategoryTag.IsValid() || Quantity <= 0)
	{
		return false;
	}

	TArray<FRetrieveItemStack>* Items = GetMutableItemsForCategory(ItemCategoryTag);
	if (!Items)
	{
		return false;
	}

	for (int32 Index = 0; Index < Items->Num(); ++Index)
	{
		FRetrieveItemStack& Stack = (*Items)[Index];
		if (Stack.ItemId != ItemId)
		{
			continue;
		}

		if (Stack.Quantity < Quantity)
		{
			return false;
		}

		Stack.Quantity -= Quantity;
		if (Stack.Quantity <= 0)
		{
			Items->RemoveAt(Index);
		}

		// 장착 중인 무기를 제거하면 전투 쪽 상태도 같이 정리
		if (ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Weapon) && EquippedWeaponId == ItemId)
		{
			if (UWeaponComponent* WeaponComp = GetWeaponComponent())
			{
				WeaponComp->UnequipWeapon();
			}
			EquippedWeaponId = NAME_None;
			OnEquippedWeaponChanged.Broadcast(EquippedWeaponId);
		}

		// 수량이 0이 된 소모품은 HUD 슬롯에서 제거
		if (ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Consumable) && !HasItem(ItemId))
		{
			for (int32 SlotKey : {4, 5})
			{
				if (GetSlotField(SlotKey) == ItemId)
				{
					GetMutableSlotField(SlotKey) = NAME_None;
					OnConsumableSlotChanged.Broadcast(SlotKey, NAME_None);
				}
			}
		}

		OnItemRemoved.Broadcast(ItemId, Quantity);
		OnInventoryChanged.Broadcast();
		return true;
	}

	return false;
}

bool UInventoryComponent::HasItem(FName ItemId, int32 Quantity) const
{
	const FRetrieveItemStack* Stack = FindStack(ItemId);
	return Stack && Stack->Quantity >= Quantity;
}

int32 UInventoryComponent::GetItemCount(FName ItemId) const
{
	const FRetrieveItemStack* Stack = FindStack(ItemId);
	return Stack ? Stack->Quantity : 0;
}

TArray<FRetrieveItemStack> UInventoryComponent::GetItemsByCategory(FGameplayTag ItemCategoryTag) const
{
	const TArray<FRetrieveItemStack>* Items = GetItemsForCategory(ItemCategoryTag);
	UE_LOG(LogTemp, Verbose, TEXT("인벤토리 카테고리 조회: Tag=%s Count=%d"),
		*ItemCategoryTag.ToString(), Items ? Items->Num() : 0);
	return Items ? *Items : TArray<FRetrieveItemStack>();
}

bool UInventoryComponent::CanOpenInventory() const
{
	const URetrieveAbilitySystemComponent* ASC = GetRetrieveAbilitySystemComponent();
	if (!ASC)
	{
		return true;
	}
	return !ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Boss_Combat)
		&& !ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Dead);
}

bool UInventoryComponent::CanChangeEquipment() const
{
	if (!CanOpenInventory())
	{
		return false;
	}
	const URetrieveAbilitySystemComponent* ASC = GetRetrieveAbilitySystemComponent();
	if (!ASC)
	{
		return true;
	}
	return !ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Combat)
		&& !ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Dodging)
		&& !ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Guarding)
		&& !ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Parrying)
		&& !ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Bursting)
		&& !ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Staggered);
}

bool UInventoryComponent::RequestEquipWeapon(FName WeaponItemId)
{
	if (!HasAuthorityToModify())
	{
		// 장착 확정은 서버에서 처리. 로컬은 가능 여부 확인 후 요청
		if (CanChangeEquipment() && HasItem(WeaponItemId))
		{
			ServerRequestEquipWeapon(WeaponItemId);
			return true;
		}
		return false;
	}

	if (!CanChangeEquipment() || !HasItem(WeaponItemId))
	{
		return false;
	}

	// Inventory는 보유/상태 검사까지만 처리. 실제 전투 반영은 WeaponComponent로 위임
	UWeaponComponent* WeaponComp = GetWeaponComponent();
	if (!WeaponComp || !WeaponComp->EquipWeapon(WeaponItemId))
	{
		return false;
	}

	EquippedWeaponId = WeaponItemId;
	OnEquippedWeaponChanged.Broadcast(EquippedWeaponId);
	OnInventoryChanged.Broadcast();
	return true;
}

bool UInventoryComponent::RequestUnequipWeapon()
{
	if (!HasAuthorityToModify())
	{
		if (CanChangeEquipment() && !EquippedWeaponId.IsNone())
		{
			ServerRequestUnequipWeapon();
			return true;
		}
		return false;
	}

	if (!CanChangeEquipment() || EquippedWeaponId.IsNone())
	{
		return false;
	}

	if (UWeaponComponent* WeaponComp = GetWeaponComponent())
	{
		WeaponComp->UnequipWeapon();
	}

	EquippedWeaponId = NAME_None;
	OnEquippedWeaponChanged.Broadcast(EquippedWeaponId);
	OnInventoryChanged.Broadcast();
	return true;
}

bool UInventoryComponent::UseConsumableItem(FName ConsumableItemId)
{
	if (!HasAuthorityToModify())
	{
		if (HasItem(ConsumableItemId))
		{
			ServerUseConsumableItem(ConsumableItemId);
			return true;
		}
		return false;
	}

	const FRetrieveItemStack* Stack = FindStack(ConsumableItemId);
	if (!Stack || !Stack->ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Consumable) || Stack->Quantity <= 0)
	{
		return false;
	}

	// 실제 회복/버프 적용은 UseItem Ability에서 처리. 현재는 UI 테스트용 수량 차감
	return RemoveItem(ConsumableItemId, Stack->ItemCategoryTag, 1);
}

bool UInventoryComponent::AssignConsumableSlot(int32 SlotKey, FName ConsumableItemId)
{
	if (!HasAuthorityToModify())
	{
		if (IsValidConsumableSlotKey(SlotKey) && HasItem(ConsumableItemId))
		{
			ServerAssignConsumableSlot(SlotKey, ConsumableItemId);
			return true;
		}
		return false;
	}

	if (!IsValidConsumableSlotKey(SlotKey) || !HasItem(ConsumableItemId))
	{
		return false;
	}

	const FRetrieveItemStack* Stack = FindStack(ConsumableItemId);
	if (!Stack || !Stack->ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Consumable))
	{
		return false;
	}

	// 같은 소모품이 다른 슬롯에 이미 배정돼 있으면 먼저 해제
	for (int32 OtherSlot : {4, 5})
	{
		if (OtherSlot != SlotKey && GetSlotField(OtherSlot) == ConsumableItemId)
		{
			GetMutableSlotField(OtherSlot) = NAME_None;
			OnConsumableSlotChanged.Broadcast(OtherSlot, NAME_None);
		}
	}

	GetMutableSlotField(SlotKey) = ConsumableItemId;
	OnConsumableSlotChanged.Broadcast(SlotKey, ConsumableItemId);
	OnInventoryChanged.Broadcast();
	return true;
}

bool UInventoryComponent::UnassignConsumableSlot(int32 SlotKey)
{
	if (!HasAuthorityToModify())
	{
		if (IsValidConsumableSlotKey(SlotKey))
		{
			ServerUnassignConsumableSlot(SlotKey);
			return true;
		}
		return false;
	}

	if (!IsValidConsumableSlotKey(SlotKey))
	{
		return false;
	}

	FName& SlotItemId = GetMutableSlotField(SlotKey);
	if (SlotItemId.IsNone())
	{
		return true;
	}

	SlotItemId = NAME_None;
	OnConsumableSlotChanged.Broadcast(SlotKey, NAME_None);
	OnInventoryChanged.Broadcast();
	return true;
}

bool UInventoryComponent::UseConsumableSlot(int32 SlotKey)
{
	if (!IsValidConsumableSlotKey(SlotKey))
	{
		return false;
	}

	const FName SlotItemId = GetSlotField(SlotKey);
	if (SlotItemId.IsNone())
	{
		return false;
	}

	if (!HasItem(SlotItemId))
	{
		return false;
	}

	UE_LOG(LogTemp, Warning, TEXT("Consumable Slot %d 사용되었음"), SlotKey);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			2.0f,
			FColor::Green,
			FString::Printf(TEXT("퀵슬롯 %d 사용되었음"), SlotKey)
		);
	}

	if (!HasAuthorityToModify())
	{
		ServerUseConsumableSlot(SlotKey);
		return true;
	}

	return UseConsumableItem(SlotItemId);
}

bool UInventoryComponent::CraftItem(FName RecipeId)
{
	// TODO: 제작 시스템 구현 예정
	UE_LOG(LogTemp, Warning, TEXT("제작 기능은 아직 구현되지 않았습니다. RecipeId=%s"), *RecipeId.ToString());
	return false;
}

FName UInventoryComponent::GetConsumableSlotItemId(int32 SlotKey) const
{
	if (!IsValidConsumableSlotKey(SlotKey))
	{
		return NAME_None;
	}
	return GetSlotField(SlotKey);
}

int32 UInventoryComponent::GetAssignedConsumableSlotKey(FName ConsumableItemId) const
{
	if (ConsumableItemId.IsNone())
	{
		return INDEX_NONE;
	}

	for (int32 SlotKey : {4, 5})
	{
		if (GetSlotField(SlotKey) == ConsumableItemId)
		{
			return SlotKey;
		}
	}
	return INDEX_NONE;
}

FRetrieveInventorySaveData UInventoryComponent::MakeInventorySaveData() const
{
	FRetrieveInventorySaveData SaveData;
	SaveData.WeaponItems = WeaponItems;
	SaveData.ConsumableItems = ConsumableItems;
	SaveData.MaterialItems = MaterialItems;
	SaveData.EquippedWeaponId = EquippedWeaponId;
	SaveData.ConsumableSlotItemIds.Add(4, ConsumableSlot4ItemId);
	SaveData.ConsumableSlotItemIds.Add(5, ConsumableSlot5ItemId);
	return SaveData;
}

bool UInventoryComponent::ApplyInventorySaveData(const FRetrieveInventorySaveData& SaveData, bool bEquipSavedWeapon)
{
	if (!HasAuthorityToModify())
	{
		return false;
	}

	WeaponItems = SaveData.WeaponItems;
	ConsumableItems = SaveData.ConsumableItems;
	MaterialItems = SaveData.MaterialItems;
	EquippedWeaponId = SaveData.EquippedWeaponId;
	ConsumableSlot4ItemId = SaveData.ConsumableSlotItemIds.FindRef(4);
	ConsumableSlot5ItemId = SaveData.ConsumableSlotItemIds.FindRef(5);

	// 예전 버전 저장 데이터나 잘못된 값 대비, 복원 직후 정리
	const FRetrieveItemStack* EquippedStack = EquippedWeaponId.IsNone() ? nullptr : FindStack(EquippedWeaponId);
	if (!EquippedStack || !EquippedStack->ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Weapon))
	{
		EquippedWeaponId = NAME_None;
	}

	for (int32 SlotKey : {4, 5})
	{
		FName& SlotItemId = GetMutableSlotField(SlotKey);
		if (SlotItemId.IsNone())
		{
			continue;
		}
		const FRetrieveItemStack* SlotStack = FindStack(SlotItemId);
		if (!SlotStack || !SlotStack->ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Consumable))
		{
			SlotItemId = NAME_None;
		}
	}

	if (bEquipSavedWeapon)
	{
		if (UWeaponComponent* WeaponComp = GetWeaponComponent())
		{
			if (EquippedWeaponId.IsNone() || !WeaponComp->EquipWeapon(EquippedWeaponId))
			{
				WeaponComp->UnequipWeapon();
				EquippedWeaponId = NAME_None;
			}
		}
	}

	OnInventoryChanged.Broadcast();
	OnEquippedWeaponChanged.Broadcast(EquippedWeaponId);
	for (int32 SlotKey : {4, 5})
	{
		OnConsumableSlotChanged.Broadcast(SlotKey, GetConsumableSlotItemId(SlotKey));
	}
	return true;
}

void UInventoryComponent::OnRep_InventoryItems()
{
	OnInventoryChanged.Broadcast();
}

void UInventoryComponent::OnRep_EquippedWeaponId()
{
	OnEquippedWeaponChanged.Broadcast(EquippedWeaponId);
	OnInventoryChanged.Broadcast();
}

void UInventoryComponent::OnRep_ConsumableSlots()
{
	for (int32 SlotKey : {4, 5})
	{
		OnConsumableSlotChanged.Broadcast(SlotKey, GetConsumableSlotItemId(SlotKey));
	}
	OnInventoryChanged.Broadcast();
}

void UInventoryComponent::ServerRequestEquipWeapon_Implementation(FName WeaponItemId)
{
	RequestEquipWeapon(WeaponItemId);
}

void UInventoryComponent::ServerRequestUnequipWeapon_Implementation()
{
	RequestUnequipWeapon();
}

void UInventoryComponent::ServerUseConsumableItem_Implementation(FName ConsumableItemId)
{
	UseConsumableItem(ConsumableItemId);
}

void UInventoryComponent::ServerAssignConsumableSlot_Implementation(int32 SlotKey, FName ConsumableItemId)
{
	AssignConsumableSlot(SlotKey, ConsumableItemId);
}

void UInventoryComponent::ServerUnassignConsumableSlot_Implementation(int32 SlotKey)
{
	UnassignConsumableSlot(SlotKey);
}

void UInventoryComponent::ServerUseConsumableSlot_Implementation(int32 SlotKey)
{
	UseConsumableSlot(SlotKey);
}

UWeaponComponent* UInventoryComponent::GetWeaponComponent() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UWeaponComponent>() : nullptr;
}

URetrieveAbilitySystemComponent* UInventoryComponent::GetRetrieveAbilitySystemComponent() const
{
	const AActor* Owner = GetOwner();
	const URetrievePawnExtensionComponent* PawnExt = Owner
		? URetrievePawnExtensionComponent::FindPawnExtensionComponent(Owner)
		: nullptr;
	return PawnExt ? PawnExt->GetRetrieveAbilitySystemComponent() : nullptr;
}

const TArray<FRetrieveItemStack>* UInventoryComponent::GetItemsForCategory(FGameplayTag ItemCategoryTag) const
{
	if (ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Weapon))    { return &WeaponItems; }
	if (ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Consumable)){ return &ConsumableItems; }
	if (ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Material))  { return &MaterialItems; }
	return nullptr;
}

TArray<FRetrieveItemStack>* UInventoryComponent::GetMutableItemsForCategory(FGameplayTag ItemCategoryTag)
{
	return const_cast<TArray<FRetrieveItemStack>*>(
		static_cast<const UInventoryComponent*>(this)->GetItemsForCategory(ItemCategoryTag));
}

const FRetrieveItemStack* UInventoryComponent::FindStack(FName ItemId) const
{
	for (const TArray<FRetrieveItemStack>* Category : { &WeaponItems, &ConsumableItems, &MaterialItems })
	{
		for (const FRetrieveItemStack& Stack : *Category)
		{
			if (Stack.ItemId == ItemId)
			{
				return &Stack;
			}
		}
	}
	return nullptr;
}

FRetrieveItemStack* UInventoryComponent::FindMutableStack(FName ItemId)
{
	return const_cast<FRetrieveItemStack*>(
		static_cast<const UInventoryComponent*>(this)->FindStack(ItemId));
}

FName& UInventoryComponent::GetMutableSlotField(int32 SlotKey)
{
	return (SlotKey == 4) ? ConsumableSlot4ItemId : ConsumableSlot5ItemId;
}

const FName& UInventoryComponent::GetSlotField(int32 SlotKey) const
{
	return (SlotKey == 4) ? ConsumableSlot4ItemId : ConsumableSlot5ItemId;
}

bool UInventoryComponent::HasAuthorityToModify() const
{
	const AActor* Owner = GetOwner();
	return !Owner || Owner->HasAuthority();
}

bool UInventoryComponent::IsValidConsumableSlotKey(int32 SlotKey)
{
	return SlotKey == 4 || SlotKey == 5;
}
