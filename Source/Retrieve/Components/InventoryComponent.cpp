#include "Components/InventoryComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/CombatAttributeSet.h"
#include "Animation/AnimMontage.h"
#include "Components/ElementGaugeComponent.h"
#include "Components/RetrievePawnExtensionComponent.h"
#include "Components/WeaponComponent.h"
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
	DOREPLIFETIME_CONDITION(UInventoryComponent, EquippedWeaponId,          COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UInventoryComponent, ConsumableSlot4ItemId,     COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UInventoryComponent, ConsumableSlot5ItemId,     COND_OwnerOnly);
	// REPNOTIFY_Always: 같은 아이템을 연속 픽업해도 OnRep가 매번 발동되도록
	DOREPLIFETIME_CONDITION_NOTIFY(UInventoryComponent, LastAddedItemNotification, COND_OwnerOnly, REPNOTIFY_Always);
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

		// 수량이 0이 된 소모품은 HUD 슬롯에서 제거
		if (ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Consumable) && !HasItem(ItemId))
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
	for (const int32 OtherSlot : ConsumableSlotKeys)
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

	if (!HasAuthorityToModify())
	{
		ServerUseConsumableSlot(SlotKey);
		return true;
	}

	return UseConsumableItem(SlotItemId);
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

	return true;
}

void UInventoryComponent::ServerCraftItem_Implementation(FName RecipeId)
{
	CraftItem(RecipeId);
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
	SaveData.EquippedWeaponId = EquippedWeaponId;
	SaveData.ConsumableSlotItemIds.Add(QuickSlotPrimaryKey, ConsumableSlot4ItemId);
	SaveData.ConsumableSlotItemIds.Add(QuickSlotSecondaryKey, ConsumableSlot5ItemId);
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
	ConsumableSlot4ItemId = SaveData.ConsumableSlotItemIds.FindRef(QuickSlotPrimaryKey);
	ConsumableSlot5ItemId = SaveData.ConsumableSlotItemIds.FindRef(QuickSlotSecondaryKey);

	// 예전 버전 저장 데이터나 잘못된 값 대비, 복원 직후 정리
	const FRetrieveItemStack* EquippedStack = EquippedWeaponId.IsNone() ? nullptr : FindStack(EquippedWeaponId);
	if (!EquippedStack || !EquippedStack->ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Weapon))
	{
		EquippedWeaponId = NAME_None;
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

	OnInventoryChanged.Broadcast();
	OnEquippedWeaponChanged.Broadcast(EquippedWeaponId);
	for (const int32 SlotKey : ConsumableSlotKeys)
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
	for (const int32 SlotKey : ConsumableSlotKeys)
	{
		OnConsumableSlotChanged.Broadcast(SlotKey, GetConsumableSlotItemId(SlotKey));
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
