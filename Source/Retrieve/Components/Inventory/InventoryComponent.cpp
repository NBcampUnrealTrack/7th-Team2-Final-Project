#include "Components/Inventory/InventoryComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/CombatAttributeSet.h"
#include "Animation/AnimMontage.h"
#include "Components/Player/ArmorComponent.h"
#include "Components/Element/ElementGaugeComponent.h"
#include "Components/Pawn/RetrievePawnExtensionComponent.h"
#include "Components/Player/WeaponComponent.h"
#include "Engine/DataTable.h"
#include "GameFramework/Character.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Net/UnrealNetwork.h"
#include "UI/HUD/RetrieveBuffUIBroadcastComponent.h"

namespace
{
constexpr int32 ConsumableSlotKeys[] = {
	UInventoryComponent::QuickSlotPrimaryKey,
	UInventoryComponent::QuickSlotSecondaryKey
};

FGameplayTag ResolveItemBuffUITag(const FRetrieveConsumableItemRow& ConsumableRow)
{
	if (ConsumableRow.BuffUITag.IsValid())
	{
		return ConsumableRow.BuffUITag;
	}

	if (ConsumableRow.ElementTag == RetrieveGameplayTags::Element_Fire)
	{
		return RetrieveGameplayTags::UI_Buff_Item_FireBoost;
	}
	if (ConsumableRow.ElementTag == RetrieveGameplayTags::Element_Water)
	{
		return RetrieveGameplayTags::UI_Buff_Item_WaterBoost;
	}
	if (ConsumableRow.ElementTag == RetrieveGameplayTags::Element_Wind)
	{
		return RetrieveGameplayTags::UI_Buff_Item_WindBoost;
	}

	return FGameplayTag();
}
}

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
	DOREPLIFETIME_CONDITION(UInventoryComponent, WeaponItems,              COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UInventoryComponent, ConsumableItems,           COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UInventoryComponent, MaterialItems,             COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UInventoryComponent, ArmorItems,                COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UInventoryComponent, EquippedWeaponId,          COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UInventoryComponent, EquippedArmorSlots,        COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UInventoryComponent, ConsumableSlot4ItemId,     COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UInventoryComponent, ConsumableSlot5ItemId,     COND_OwnerOnly);
	// REPNOTIFY_Always: 같은 아이템을 연속 픽업해도 OnRep가 매번 발동되도록
	DOREPLIFETIME_CONDITION_NOTIFY(UInventoryComponent, LastAddedItemNotification, COND_OwnerOnly, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION(UInventoryComponent, Currency, COND_OwnerOnly);
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

	if (ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Weapon)
		|| ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Armor))
	{
		// 무기/방어구는 같은 ItemId 중복 적재 없이 성공 처리
		if (FindStack(ItemId))
		{
			return true;
		}

		FRetrieveItemStack& NewStack = Items->AddDefaulted_GetRef();
		NewStack.ItemId = ItemId;
		NewStack.ItemCategoryTag = ItemCategoryTag;
		NewStack.Quantity = 1;
		UE_LOG(LogTemp, Log, TEXT("인벤토리 장비 추가: ItemId=%s Quantity=1"), *ItemId.ToString());
		OnItemAdded.Broadcast(ItemId, ItemCategoryTag, 1);
		OnInventoryChanged.Broadcast();
		// 클라이언트 토스트 알림 — OnRep_LastAddedItem → OnItemAdded.Broadcast (클라이언트측)
		LastAddedItemNotification.ItemId = ItemId;
		LastAddedItemNotification.ItemCategoryTag = ItemCategoryTag;
		LastAddedItemNotification.Quantity = 1;
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

	OnItemAdded.Broadcast(ItemId, ItemCategoryTag, Quantity);
	OnInventoryChanged.Broadcast();
	// 클라이언트 토스트 알림 — OnRep_LastAddedItem → OnItemAdded.Broadcast (클라이언트측)
	LastAddedItemNotification.ItemId = ItemId;
	LastAddedItemNotification.ItemCategoryTag = ItemCategoryTag;
	LastAddedItemNotification.Quantity = Quantity;
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

		// 장착 중인 방어구를 제거하면 ArmorComponent의 공개 장착 상태도 같이 정리
		if (ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Armor) && !HasItem(ItemId))
		{
			for (int32 SlotIndex = EquippedArmorSlots.Num() - 1; SlotIndex >= 0; --SlotIndex)
			{
				if (EquippedArmorSlots[SlotIndex].ArmorItemId != ItemId)
				{
					continue;
				}

				const FGameplayTag EquipmentSlotTag = EquippedArmorSlots[SlotIndex].EquipmentSlotTag;
				if (UArmorComponent* ArmorComp = GetArmorComponent())
				{
					ArmorComp->UnequipArmor(EquipmentSlotTag);
				}

				EquippedArmorSlots.RemoveAt(SlotIndex);
				OnEquippedArmorChanged.Broadcast(EquipmentSlotTag, NAME_None);
			}
		}

		// 수량이 0이 된 소모품/무기는 퀵슬롯에서 제거
		if (!HasItem(ItemId))
		{
			if (ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Consumable))
			{
				for (const int32 SlotKey : ConsumableSlotKeys)
				{
					if (GetSlotField(SlotKey) == ItemId)
					{
						GetMutableSlotField(SlotKey) = NAME_None;
						OnConsumableSlotChanged.Broadcast(SlotKey, NAME_None);
					}
				}
			}

			for (auto It = QuickSlots.CreateIterator(); It; ++It)
			{
				if (It.Value().ItemId == ItemId)
				{
					const int32 SlotKey = It.Key();
					FRetrieveQuickSlotEntry EmptyEntry;
					EmptyEntry.SlotKey = SlotKey;
					OnQuickSlotChanged.Broadcast(SlotKey, EmptyEntry);
					It.RemoveCurrent();
				}
			}
		}

		OnItemRemoved.Broadcast(ItemId, ItemCategoryTag, Quantity);
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

bool UInventoryComponent::RequestEquipArmor(FGameplayTag EquipmentSlotTag, FName ArmorItemId)
{
	UE_LOG(LogTemp, Log, TEXT("[RequestEquipArmor] called — Slot=%s ItemId=%s HasAuthority=%d"),
		*EquipmentSlotTag.ToString(), *ArmorItemId.ToString(), HasAuthorityToModify() ? 1 : 0);

	if (!HasAuthorityToModify())
	{
		if (CanChangeEquipment() && EquipmentSlotTag.IsValid() && HasItem(ArmorItemId))
		{
			ServerRequestEquipArmor(EquipmentSlotTag, ArmorItemId);
			return true;
		}
		UE_LOG(LogTemp, Warning, TEXT("[RequestEquipArmor] no authority — CanChangeEquip=%d HasItem=%d"),
			CanChangeEquipment() ? 1 : 0, HasItem(ArmorItemId) ? 1 : 0);
		return false;
	}

	if (!CanChangeEquipment() || !EquipmentSlotTag.IsValid() || !HasItem(ArmorItemId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[RequestEquipArmor] FAIL — CanChangeEquip=%d SlotValid=%d HasItem=%d"),
			CanChangeEquipment() ? 1 : 0, EquipmentSlotTag.IsValid() ? 1 : 0, HasItem(ArmorItemId) ? 1 : 0);
		return false;
	}

	const FRetrieveItemStack* Stack = FindStack(ArmorItemId);
	if (!Stack || !Stack->ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Armor))
	{
		UE_LOG(LogTemp, Warning, TEXT("[RequestEquipArmor] FAIL — Stack not found or CategoryTag mismatch (tag=%s)"),
			Stack ? *Stack->ItemCategoryTag.ToString() : TEXT("null"));
		return false;
	}

	UArmorComponent* ArmorComp = GetArmorComponent();
	if (!ArmorComp || !ArmorComp->EquipArmor(EquipmentSlotTag, ArmorItemId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[RequestEquipArmor] FAIL — ArmorComp=%d EquipArmor failed"), ArmorComp ? 1 : 0);
		return false;
	}

	if (FRetrieveEquippedArmorEntry* ExistingSlot = FindMutableEquippedArmorSlot(EquipmentSlotTag))
	{
		ExistingSlot->ArmorItemId = ArmorItemId;
	}
	else
	{
		FRetrieveEquippedArmorEntry& NewSlot = EquippedArmorSlots.AddDefaulted_GetRef();
		NewSlot.EquipmentSlotTag = EquipmentSlotTag;
		NewSlot.ArmorItemId = ArmorItemId;
	}

	OnEquippedArmorChanged.Broadcast(EquipmentSlotTag, ArmorItemId);
	OnInventoryChanged.Broadcast();
	return true;
}

bool UInventoryComponent::RequestUnequipArmor(FGameplayTag EquipmentSlotTag)
{
	if (!HasAuthorityToModify())
	{
		if (CanChangeEquipment() && !GetEquippedArmorId(EquipmentSlotTag).IsNone())
		{
			ServerRequestUnequipArmor(EquipmentSlotTag);
			return true;
		}
		return false;
	}

	if (!CanChangeEquipment() || !EquipmentSlotTag.IsValid())
	{
		return false;
	}

	if (GetEquippedArmorId(EquipmentSlotTag).IsNone())
	{
		return false;
	}

	if (UArmorComponent* ArmorComp = GetArmorComponent())
	{
		ArmorComp->UnequipArmor(EquipmentSlotTag);
	}

	RemoveEquippedArmorSlot(EquipmentSlotTag);
	OnEquippedArmorChanged.Broadcast(EquipmentSlotTag, NAME_None);
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

	const FRetrieveConsumableItemRow* ConsumableRow = FindConsumableItemRow(ConsumableItemId);
	if (!ConsumableRow || !ApplyConsumableEffects(*ConsumableRow))
	{
		return false;
	}

	return RemoveItem(ConsumableItemId, Stack->ItemCategoryTag, 1);
}

bool UInventoryComponent::AssignConsumableSlot(int32 SlotKey, FName ConsumableItemId)
{
	return AssignQuickSlotItem(SlotKey, ConsumableItemId, RetrieveGameplayTags::Item_Consumable);
}

bool UInventoryComponent::UnassignConsumableSlot(int32 SlotKey)
{
	return UnassignQuickSlotItem(SlotKey);
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

	// 클라이언트 즉시 UI 피드백 — 서버 권한 여부와 무관하게 브로드캐스트
	OnConsumableSlotUsed.Broadcast(SlotKey);

	if (!HasAuthorityToModify())
	{
		ServerUseConsumableSlot(SlotKey);
		return true;
	}

	return UseConsumableItem(SlotItemId);
}

bool UInventoryComponent::AssignQuickSlotItem(int32 SlotKey, FName ItemId, FGameplayTag ItemCategoryTag)
{
	if (!HasAuthorityToModify())
	{
		if (SlotKey != INDEX_NONE && !ItemId.IsNone() && ItemCategoryTag.IsValid() && HasItem(ItemId))
		{
			// QuickSlots(TMap)는 복제되지 않으므로 클라이언트 로컬을 즉시 갱신(예측)하고
			// 서버 RPC로 권위 측에도 반영한다. (슬롯 0~3은 복제 필드가 없어 이 갱신이 필수)
			FRetrieveQuickSlotEntry PredictedEntry;
			PredictedEntry.SlotKey = SlotKey;
			PredictedEntry.ItemId = ItemId;
			PredictedEntry.ItemCategoryTag = ItemCategoryTag;
			QuickSlots.Add(SlotKey, PredictedEntry);
			OnQuickSlotChanged.Broadcast(SlotKey, PredictedEntry);

			if (ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Consumable) && IsValidConsumableSlotKey(SlotKey))
			{
				OnConsumableSlotChanged.Broadcast(SlotKey, ItemId);
			}

			ServerAssignQuickSlotItem(SlotKey, ItemId, ItemCategoryTag);
			return true;
		}
		return false;
	}

	if (SlotKey == INDEX_NONE || ItemId.IsNone() || !ItemCategoryTag.IsValid())
	{
		return false;
	}

	if (GetItemCount(ItemId) <= 0)
	{
		return false;
	}

	if (!ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Weapon)
		&& !ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Consumable))
	{
		return false;
	}

	// 소모품인 경우: 같은 아이템이 다른 슬롯에 있으면 먼저 해제
	if (ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Consumable))
	{
		for (auto It = QuickSlots.CreateIterator(); It; ++It)
		{
			if (It.Key() != SlotKey && It.Value().ItemId == ItemId
				&& It.Value().ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Consumable))
			{
				const int32 OtherSlot = It.Key();
				FRetrieveQuickSlotEntry EmptyEntry;
				EmptyEntry.SlotKey = OtherSlot;
				OnQuickSlotChanged.Broadcast(OtherSlot, EmptyEntry);
				if (IsValidConsumableSlotKey(OtherSlot))
				{
					GetMutableSlotField(OtherSlot) = NAME_None;
					OnConsumableSlotChanged.Broadcast(OtherSlot, NAME_None);
				}
				It.RemoveCurrent();
			}
		}
		// 기존 복제 필드도 같은 아이템이면 해제
		for (const int32 OtherSlot : ConsumableSlotKeys)
		{
			if (OtherSlot != SlotKey && GetSlotField(OtherSlot) == ItemId)
			{
				GetMutableSlotField(OtherSlot) = NAME_None;
				OnConsumableSlotChanged.Broadcast(OtherSlot, NAME_None);
			}
		}
	}

	FRetrieveQuickSlotEntry NewEntry;
	NewEntry.SlotKey = SlotKey;
	NewEntry.ItemId = ItemId;
	NewEntry.ItemCategoryTag = ItemCategoryTag;
	QuickSlots.Add(SlotKey, NewEntry);

	OnQuickSlotChanged.Broadcast(SlotKey, NewEntry);

	// 슬롯 4&5 소모품은 복제 필드도 동기화 (기존 HUD/QuickSlotBar 호환)
	if (ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Consumable) && IsValidConsumableSlotKey(SlotKey))
	{
		GetMutableSlotField(SlotKey) = ItemId;
		OnConsumableSlotChanged.Broadcast(SlotKey, ItemId);
	}

	OnInventoryChanged.Broadcast();
	return true;
}

bool UInventoryComponent::UnassignQuickSlotItem(int32 SlotKey)
{
	if (!HasAuthorityToModify())
	{
		if (QuickSlots.Contains(SlotKey)
			|| (IsValidConsumableSlotKey(SlotKey) && !GetSlotField(SlotKey).IsNone()))
		{
			// 클라이언트 로컬 예측 갱신 후 서버에도 반영 (TMap 비복제 대응)
			QuickSlots.Remove(SlotKey);

			FRetrieveQuickSlotEntry EmptyEntry;
			EmptyEntry.SlotKey = SlotKey;
			OnQuickSlotChanged.Broadcast(SlotKey, EmptyEntry);

			if (IsValidConsumableSlotKey(SlotKey))
			{
				OnConsumableSlotChanged.Broadcast(SlotKey, NAME_None);
			}

			ServerUnassignQuickSlotItem(SlotKey);
			return true;
		}
		return false;
	}

	const bool bInMap = QuickSlots.Contains(SlotKey);
	const bool bHasReplicatedSlot =
		IsValidConsumableSlotKey(SlotKey) && !GetSlotField(SlotKey).IsNone();

	if (!bInMap && !bHasReplicatedSlot)
	{
		return false;
	}

	QuickSlots.Remove(SlotKey);

	FRetrieveQuickSlotEntry EmptyEntry;
	EmptyEntry.SlotKey = SlotKey;
	OnQuickSlotChanged.Broadcast(SlotKey, EmptyEntry);

	if (IsValidConsumableSlotKey(SlotKey))
	{
		GetMutableSlotField(SlotKey) = NAME_None;
	}

	OnConsumableSlotChanged.Broadcast(SlotKey, NAME_None);
	OnInventoryChanged.Broadcast();
	return true;
}

FRetrieveQuickSlotEntry UInventoryComponent::GetQuickSlotEntry(int32 SlotKey) const
{
	if (const FRetrieveQuickSlotEntry* Entry = QuickSlots.Find(SlotKey))
	{
		return *Entry;
	}

	// 복제 필드(슬롯 4&5)에 소모품이 있으면 엔트리 구성
	if (IsValidConsumableSlotKey(SlotKey))
	{
		const FName ReplicatedItemId = GetSlotField(SlotKey);
		if (!ReplicatedItemId.IsNone())
		{
			FRetrieveQuickSlotEntry Entry;
			Entry.SlotKey = SlotKey;
			Entry.ItemId = ReplicatedItemId;
			Entry.ItemCategoryTag = RetrieveGameplayTags::Item_Consumable;
			return Entry;
		}
	}

	FRetrieveQuickSlotEntry EmptyEntry;
	EmptyEntry.SlotKey = SlotKey;
	return EmptyEntry;
}

int32 UInventoryComponent::GetAssignedQuickSlotKey(FName ItemId, FGameplayTag ItemCategoryTag) const
{
	if (ItemId.IsNone() || !ItemCategoryTag.IsValid())
	{
		return INDEX_NONE;
	}

	for (const TPair<int32, FRetrieveQuickSlotEntry>& Pair : QuickSlots)
	{
		const FRetrieveQuickSlotEntry& Entry = Pair.Value;
		if (Entry.ItemId == ItemId && Entry.ItemCategoryTag == ItemCategoryTag)
		{
			return Pair.Key;
		}
	}

	// 복제 필드 fallback (슬롯 4&5 소모품)
	if (ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Consumable))
	{
		for (const int32 SlotKey : ConsumableSlotKeys)
		{
			if (GetSlotField(SlotKey) == ItemId)
			{
				return SlotKey;
			}
		}
	}

	return INDEX_NONE;
}

bool UInventoryComponent::ActivateQuickSlotItem(int32 SlotKey)
{
	const FRetrieveQuickSlotEntry Entry = GetQuickSlotEntry(SlotKey);
	if (!Entry.IsValid())
	{
		return false;
	}

	if (Entry.ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Weapon))
	{
		return RequestEquipWeapon(Entry.ItemId);
	}

	if (Entry.ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Consumable))
	{
		return UseConsumableItem(Entry.ItemId);
	}

	return false;
}

void UInventoryComponent::ServerAssignQuickSlotItem_Implementation(
	int32 SlotKey, FName ItemId, FGameplayTag ItemCategoryTag)
{
	AssignQuickSlotItem(SlotKey, ItemId, ItemCategoryTag);
}

void UInventoryComponent::ServerUnassignQuickSlotItem_Implementation(int32 SlotKey)
{
	UnassignQuickSlotItem(SlotKey);
}

bool UInventoryComponent::CanCraftItem(FName RecipeId) const
{
	const FRetrieveCraftRecipeRow* Recipe = FindCraftRecipeRow(RecipeId, TEXT("CanCraftItem"));
	if (!Recipe || !IsCraftRecipeValid(*Recipe)) { return false; }

	return HasRequiredCraftMaterials(*Recipe);
}

int32 UInventoryComponent::GetMaxCraftableCount(FName RecipeId) const
{
	const FRetrieveCraftRecipeRow* Recipe = FindCraftRecipeRow(RecipeId, TEXT("GetMaxCraftableCount"));
	if (!Recipe || !IsCraftRecipeValid(*Recipe) || Recipe->RequiredMaterials.IsEmpty()) { return 0; }

	TMap<FName, int32> RequiredCounts;
	for (const FRetrieveItemStack& Material : Recipe->RequiredMaterials)
	{
		RequiredCounts.FindOrAdd(Material.ItemId) += Material.Quantity;
	}

	int32 MaxCount = TNumericLimits<int32>::Max();
	for (const TPair<FName, int32>& Requirement : RequiredCounts)
	{
		MaxCount = FMath::Min(MaxCount, GetItemCount(Requirement.Key) / Requirement.Value);
	}
	return MaxCount == TNumericLimits<int32>::Max() ? 0 : MaxCount;
}

FText UInventoryComponent::GetCraftRecipeDisplayName(FName RecipeId) const
{
	const FRetrieveCraftRecipeRow* Recipe = FindCraftRecipeRow(RecipeId, TEXT("GetCraftRecipeDisplayName"));
	return Recipe && !Recipe->DisplayName.IsEmpty()
		? Recipe->DisplayName
		: FText::FromName(RecipeId);
}

bool UInventoryComponent::CraftItem(FName RecipeId)
{
	if (!HasAuthorityToModify())
	{
		ServerCraftItem(RecipeId);
		return true;
	}

	const FRetrieveCraftRecipeRow* Recipe = FindCraftRecipeRow(RecipeId, TEXT("CraftItem"));
	if (!Recipe || !IsCraftRecipeValid(*Recipe))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CraftItem] Invalid recipe data. RecipeId=%s"), *RecipeId.ToString());
		return false;
	}

	if (!HasRequiredCraftMaterials(*Recipe))
	{
		return false;
	}

	if (!ConsumeCraftMaterials(*Recipe))
	{
		return false;
	}

	if (!AddItem(Recipe->OutputItem.ItemId, Recipe->OutputItem.ItemCategoryTag, Recipe->OutputItem.Quantity))
	{
		UE_LOG(LogTemp, Error, TEXT("[CraftItem] Failed to add output item. RecipeId=%s"), *RecipeId.ToString());
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[CraftItem] 제작 성공 RecipeId=%s → %s x%d"),
		*RecipeId.ToString(), *Recipe->OutputItem.ItemId.ToString(), Recipe->OutputItem.Quantity);

	OnCraftCompleted.Broadcast(true, RecipeId, Recipe->OutputItem.ItemId);
	return true;
}

void UInventoryComponent::ServerCraftItem_Implementation(FName RecipeId)
{
	CraftItem(RecipeId);
}

FName UInventoryComponent::GetEquippedArmorId(FGameplayTag EquipmentSlotTag) const
{
	const FRetrieveEquippedArmorEntry* ArmorSlot = FindEquippedArmorSlot(EquipmentSlotTag);
	return ArmorSlot ? ArmorSlot->ArmorItemId : NAME_None;
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

	for (const int32 SlotKey : ConsumableSlotKeys)
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
	SaveData.ArmorItems = ArmorItems;
	SaveData.EquippedWeaponId = EquippedWeaponId;
	SaveData.EquippedArmorSlots = EquippedArmorSlots;
	SaveData.ConsumableSlotItemIds.Add(QuickSlotPrimaryKey, ConsumableSlot4ItemId);
	SaveData.ConsumableSlotItemIds.Add(QuickSlotSecondaryKey, ConsumableSlot5ItemId);
	SaveData.Currency = Currency;
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
	ArmorItems = SaveData.ArmorItems;
	EquippedWeaponId = SaveData.EquippedWeaponId;
	EquippedArmorSlots = SaveData.EquippedArmorSlots;
	ConsumableSlot4ItemId = SaveData.ConsumableSlotItemIds.FindRef(QuickSlotPrimaryKey);
	ConsumableSlot5ItemId = SaveData.ConsumableSlotItemIds.FindRef(QuickSlotSecondaryKey);

	// 예전 버전 저장 데이터나 잘못된 값 대비, 복원 직후 정리
	const FRetrieveItemStack* EquippedStack = EquippedWeaponId.IsNone() ? nullptr : FindStack(EquippedWeaponId);
	if (!EquippedStack || !EquippedStack->ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Weapon))
	{
		EquippedWeaponId = NAME_None;
	}

	for (int32 Index = EquippedArmorSlots.Num() - 1; Index >= 0; --Index)
	{
		const FRetrieveEquippedArmorEntry& ArmorSlot = EquippedArmorSlots[Index];
		const FRetrieveItemStack* ArmorStack = ArmorSlot.ArmorItemId.IsNone()
			? nullptr
			: FindStack(ArmorSlot.ArmorItemId);
		if (!ArmorSlot.EquipmentSlotTag.IsValid()
			|| !ArmorStack
			|| !ArmorStack->ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Armor))
		{
			EquippedArmorSlots.RemoveAt(Index);
		}
	}

	for (const int32 SlotKey : ConsumableSlotKeys)
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

	if (UArmorComponent* ArmorComp = GetArmorComponent())
	{
		ArmorComp->UnequipAllArmor();
		for (const FRetrieveEquippedArmorEntry& ArmorSlot : EquippedArmorSlots)
		{
			ArmorComp->EquipArmor(ArmorSlot.EquipmentSlotTag, ArmorSlot.ArmorItemId);
		}
	}

	OnInventoryChanged.Broadcast();
	OnEquippedWeaponChanged.Broadcast(EquippedWeaponId);
	for (const FRetrieveEquippedArmorEntry& ArmorSlot : EquippedArmorSlots)
	{
		OnEquippedArmorChanged.Broadcast(ArmorSlot.EquipmentSlotTag, ArmorSlot.ArmorItemId);
	}
	// 복제 필드에서 QuickSlots TMap 복원 및 브로드캐스트
	QuickSlots.Reset();
	for (const int32 SlotKey : ConsumableSlotKeys)
	{
		const FName ItemId = GetConsumableSlotItemId(SlotKey);
		OnConsumableSlotChanged.Broadcast(SlotKey, ItemId);
		if (!ItemId.IsNone())
		{
			FRetrieveQuickSlotEntry Entry;
			Entry.SlotKey = SlotKey;
			Entry.ItemId = ItemId;
			Entry.ItemCategoryTag = RetrieveGameplayTags::Item_Consumable;
			QuickSlots.Add(SlotKey, Entry);
			OnQuickSlotChanged.Broadcast(SlotKey, Entry);
		}
	}
	Currency = SaveData.Currency;
	OnCurrencyChanged.Broadcast(Currency);
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

void UInventoryComponent::OnRep_EquippedArmorSlots()
{
	for (const FRetrieveEquippedArmorEntry& ArmorSlot : EquippedArmorSlots)
	{
		OnEquippedArmorChanged.Broadcast(ArmorSlot.EquipmentSlotTag, ArmorSlot.ArmorItemId);
	}
	OnInventoryChanged.Broadcast();
}

void UInventoryComponent::OnRep_ConsumableSlots()
{
	for (const int32 SlotKey : ConsumableSlotKeys)
	{
		const FName ItemId = GetConsumableSlotItemId(SlotKey);
		OnConsumableSlotChanged.Broadcast(SlotKey, ItemId);

		// QuickSlots TMap 클라이언트 동기화
		if (!ItemId.IsNone())
		{
			FRetrieveQuickSlotEntry Entry;
			Entry.SlotKey = SlotKey;
			Entry.ItemId = ItemId;
			Entry.ItemCategoryTag = RetrieveGameplayTags::Item_Consumable;
			QuickSlots.Add(SlotKey, Entry);
			OnQuickSlotChanged.Broadcast(SlotKey, Entry);
		}
		else
		{
			QuickSlots.Remove(SlotKey);
			FRetrieveQuickSlotEntry EmptyEntry;
			EmptyEntry.SlotKey = SlotKey;
			OnQuickSlotChanged.Broadcast(SlotKey, EmptyEntry);
		}
	}
	OnInventoryChanged.Broadcast();
}

void UInventoryComponent::OnRep_LastAddedItem()
{
	// 전용 서버에서 클라이언트로 복제될 때 호출됨.
	// 리슨 서버 호스트와 스탠드얼론은 AddItem에서 직접 Broadcast되므로 여기 진입 안 함.
	if (!LastAddedItemNotification.ItemId.IsNone())
	{
		OnItemAdded.Broadcast(
			LastAddedItemNotification.ItemId,
			LastAddedItemNotification.ItemCategoryTag,
			LastAddedItemNotification.Quantity);
	}
}

// ── Currency ──────────────────────────────────────────────────────────────────

bool UInventoryComponent::AddCurrency(int32 Amount)
{
	if (Amount <= 0) return false;
	if (!HasAuthorityToModify())
	{
		ServerAddCurrency(Amount);
		return true;
	}
	Currency += Amount;
	OnCurrencyChanged.Broadcast(Currency);
	return true;
}

bool UInventoryComponent::SpendCurrency(int32 Amount)
{
	if (Amount <= 0 || Currency < Amount) return false;
	if (!HasAuthorityToModify())
	{
		ServerSpendCurrency(Amount);
		return true;
	}
	Currency -= Amount;
	OnCurrencyChanged.Broadcast(Currency);
	return true;
}

void UInventoryComponent::OnRep_Currency()
{
	OnCurrencyChanged.Broadcast(Currency);
}

void UInventoryComponent::ServerAddCurrency_Implementation(int32 Amount)
{
	AddCurrency(Amount);
}

void UInventoryComponent::ServerSpendCurrency_Implementation(int32 Amount)
{
	SpendCurrency(Amount);
}

void UInventoryComponent::ServerRequestEquipWeapon_Implementation(FName WeaponItemId)
{
	RequestEquipWeapon(WeaponItemId);
}

void UInventoryComponent::ServerRequestUnequipWeapon_Implementation()
{
	RequestUnequipWeapon();
}

void UInventoryComponent::ServerRequestEquipArmor_Implementation(FGameplayTag EquipmentSlotTag, FName ArmorItemId)
{
	RequestEquipArmor(EquipmentSlotTag, ArmorItemId);
}

void UInventoryComponent::ServerRequestUnequipArmor_Implementation(FGameplayTag EquipmentSlotTag)
{
	RequestUnequipArmor(EquipmentSlotTag);
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

UArmorComponent* UInventoryComponent::GetArmorComponent() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<UArmorComponent>() : nullptr;
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

const FRetrieveConsumableItemRow* UInventoryComponent::FindConsumableItemRow(FName ConsumableItemId) const
{
	UDataTable* Table = ConsumableItemTable;
	if (!Table)
	{
		Table = LoadObject<UDataTable>(
			nullptr,
			TEXT("/Game/Retrieve/Data/Items/DT_ConsumableItem.DT_ConsumableItem"));
	}

	return Table
		? Table->FindRow<FRetrieveConsumableItemRow>(ConsumableItemId, TEXT("InventoryComponent::FindConsumableItemRow"))
		: nullptr;
}

const FRetrieveCraftRecipeRow* UInventoryComponent::FindCraftRecipeRow(FName RecipeId, const TCHAR* Context) const
{
	if (RecipeId.IsNone())
	{
		return nullptr;
	}

	if (UDataTable* Table = ResolveCraftRecipeTable())
	{
		if (const FRetrieveCraftRecipeRow* Recipe = Table->FindRow<FRetrieveCraftRecipeRow>(
			RecipeId, Context, false))
		{
			return Recipe;
		}
	}

	UDataTable* DefaultTable = LoadObject<UDataTable>(
		nullptr,
		TEXT("/Game/Retrieve/Data/Items/DT_CraftRecipe.DT_CraftRecipe"));
	return DefaultTable && DefaultTable != CraftRecipeTable
		? DefaultTable->FindRow<FRetrieveCraftRecipeRow>(RecipeId, Context)
		: nullptr;
}

UDataTable* UInventoryComponent::ResolveCraftRecipeTable() const
{
	return CraftRecipeTable
		? CraftRecipeTable.Get()
		: LoadObject<UDataTable>(
			nullptr,
			TEXT("/Game/Retrieve/Data/Items/DT_CraftRecipe.DT_CraftRecipe"));
}

bool UInventoryComponent::IsCraftRecipeValid(const FRetrieveCraftRecipeRow& Recipe) const
{
	if (Recipe.OutputItem.ItemId.IsNone()
		|| !Recipe.OutputItem.ItemCategoryTag.IsValid()
		|| Recipe.OutputItem.Quantity <= 0
		|| !GetItemsForCategory(Recipe.OutputItem.ItemCategoryTag)
		|| Recipe.RequiredMaterials.IsEmpty())
	{
		return false;
	}

	for (const FRetrieveItemStack& Material : Recipe.RequiredMaterials)
	{
		if (Material.ItemId.IsNone()
			|| !Material.ItemCategoryTag.IsValid()
			|| Material.Quantity <= 0
			|| !GetItemsForCategory(Material.ItemCategoryTag))
		{
			return false;
		}
	}
	return true;
}

bool UInventoryComponent::HasRequiredCraftMaterials(const FRetrieveCraftRecipeRow& Recipe) const
{
	TMap<FName, int32> RequiredCounts;
	for (const FRetrieveItemStack& Material : Recipe.RequiredMaterials)
	{
		RequiredCounts.FindOrAdd(Material.ItemId) += Material.Quantity;
	}

	for (const TPair<FName, int32>& Requirement : RequiredCounts)
	{
		if (GetItemCount(Requirement.Key) < Requirement.Value)
		{
			return false;
		}
	}
	return true;
}

bool UInventoryComponent::ConsumeCraftMaterials(const FRetrieveCraftRecipeRow& Recipe)
{
	TMap<FName, FRetrieveItemStack> RequiredMaterials;
	for (const FRetrieveItemStack& Material : Recipe.RequiredMaterials)
	{
		if (FRetrieveItemStack* Requirement = RequiredMaterials.Find(Material.ItemId))
		{
			Requirement->Quantity += Material.Quantity;
		}
		else
		{
			RequiredMaterials.Add(Material.ItemId, Material);
		}
	}

	for (const TPair<FName, FRetrieveItemStack>& Requirement : RequiredMaterials)
	{
		const FRetrieveItemStack& Material = Requirement.Value;
		if (!RemoveItem(Material.ItemId, Material.ItemCategoryTag, Material.Quantity))
		{
			return false;
		}
	}
	return true;
}

bool UInventoryComponent::ApplyConsumableEffects(const FRetrieveConsumableItemRow& ConsumableRow)
{
	URetrieveAbilitySystemComponent* ASC = GetRetrieveAbilitySystemComponent();
	if (!ASC || ASC->HasAnyMatchingGameplayTags(ConsumableRow.BlockedStateTags))
	{
		return false;
	}

	bool bAppliedEffect = false;
	const FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();

	if (const UGameplayEffect* HealEffect = ConsumableRow.HealEffect.GetDefaultObject())
	{
		ASC->ApplyGameplayEffectToSelf(HealEffect, 1.0f, EffectContext);
		bAppliedEffect = true;
	}
	else if (ConsumableRow.HealAmount > 0.0f)
	{
		UGameplayEffect* RuntimeHealEffect = NewObject<UGameplayEffect>(GetTransientPackage());
		RuntimeHealEffect->DurationPolicy = EGameplayEffectDurationType::Instant;

		FGameplayModifierInfo& HealingModifier = RuntimeHealEffect->Modifiers.AddDefaulted_GetRef();
		HealingModifier.Attribute = UCombatAttributeSet::GetIncomingHealingAttribute();
		HealingModifier.ModifierOp = EGameplayModOp::Additive;
		HealingModifier.ModifierMagnitude = FScalableFloat(ConsumableRow.HealAmount);

		ASC->ApplyGameplayEffectToSelf(RuntimeHealEffect, 1.0f, EffectContext);
		bAppliedEffect = true;
	}

	if (const UGameplayEffect* ElementBuffEffect = ConsumableRow.ElementBuffEffect.GetDefaultObject())
	{
		ASC->ApplyGameplayEffectToSelf(ElementBuffEffect, 1.0f, EffectContext);
		bAppliedEffect = true;
	}

	// 아이템 원소 충전 배율 적용 (ElementBuffMultiplier > 1.0인 경우)
	if (ConsumableRow.ElementBuffMultiplier > 0.f && ConsumableRow.ElementTag.IsValid())
	{
		bAppliedEffect = true;

		const FGameplayTag BuffUITag = ResolveItemBuffUITag(ConsumableRow);
		if (UElementGaugeComponent* GaugeComp = GetOwner()->FindComponentByClass<UElementGaugeComponent>())
		{
			GaugeComp->SetItemChargeMultiplier(ConsumableRow.ElementBuffMultiplier, ConsumableRow.BuffDuration, BuffUITag);
		}

		if (URetrieveBuffUIBroadcastComponent* BuffUI = GetOwner()->FindComponentByClass<URetrieveBuffUIBroadcastComponent>())
		{
			BuffUI->BroadcastBuffManual(BuffUITag, ConsumableRow.BuffDuration);
		}
	}

	if (!bAppliedEffect)
	{
		return false;
	}

	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UAnimMontage* UseMontage = ConsumableRow.UseMontage.LoadSynchronous())
		{
			Character->PlayAnimMontage(UseMontage);
		}
	}

	FGameplayEventData EventData;
	EventData.EventTag = RetrieveGameplayTags::GameplayEvent_Item_Used;
	EventData.Instigator = GetOwner();
	EventData.Target = GetOwner();
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		GetOwner(),
		RetrieveGameplayTags::GameplayEvent_Item_Used,
		EventData);

	return true;
}

const TArray<FRetrieveItemStack>* UInventoryComponent::GetItemsForCategory(FGameplayTag ItemCategoryTag) const
{
	if (ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Weapon))    { return &WeaponItems; }
	if (ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Armor))     { return &ArmorItems; }
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
	for (const TArray<FRetrieveItemStack>* Category : { &WeaponItems, &ArmorItems, &ConsumableItems, &MaterialItems })
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

const FRetrieveEquippedArmorEntry* UInventoryComponent::FindEquippedArmorSlot(FGameplayTag EquipmentSlotTag) const
{
	for (const FRetrieveEquippedArmorEntry& ArmorSlot : EquippedArmorSlots)
	{
		if (ArmorSlot.EquipmentSlotTag == EquipmentSlotTag)
		{
			return &ArmorSlot;
		}
	}
	return nullptr;
}

FRetrieveEquippedArmorEntry* UInventoryComponent::FindMutableEquippedArmorSlot(FGameplayTag EquipmentSlotTag)
{
	return const_cast<FRetrieveEquippedArmorEntry*>(
		static_cast<const UInventoryComponent*>(this)->FindEquippedArmorSlot(EquipmentSlotTag));
}

void UInventoryComponent::RemoveEquippedArmorSlot(FGameplayTag EquipmentSlotTag)
{
	for (int32 Index = 0; Index < EquippedArmorSlots.Num(); ++Index)
	{
		if (EquippedArmorSlots[Index].EquipmentSlotTag == EquipmentSlotTag)
		{
			EquippedArmorSlots.RemoveAt(Index);
			return;
		}
	}
}

FName& UInventoryComponent::GetMutableSlotField(int32 SlotKey)
{
	return SlotKey == QuickSlotPrimaryKey ? ConsumableSlot4ItemId : ConsumableSlot5ItemId;
}

const FName& UInventoryComponent::GetSlotField(int32 SlotKey) const
{
	return SlotKey == QuickSlotPrimaryKey ? ConsumableSlot4ItemId : ConsumableSlot5ItemId;
}

bool UInventoryComponent::HasAuthorityToModify() const
{
	const AActor* Owner = GetOwner();
	return !Owner || Owner->HasAuthority();
}

bool UInventoryComponent::IsValidConsumableSlotKey(int32 SlotKey)
{
	return SlotKey == QuickSlotPrimaryKey || SlotKey == QuickSlotSecondaryKey;
}
