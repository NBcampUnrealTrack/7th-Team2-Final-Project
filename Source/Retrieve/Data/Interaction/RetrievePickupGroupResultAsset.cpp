#include "Data/Interaction/RetrievePickupGroupResultAsset.h"

#include "Components/InventoryComponent.h"
#include "GameFramework/Actor.h"

void URetrievePickupGroupResultAsset::ApplyResult_Implementation(
	UObject* /*WorldContextObject*/,
	AActor* Instigator,
	AActor* /*InteractedActor*/) const
{
	if (!Instigator)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Retrieve|PickupGroup] Instigator가 nullptr이라 적용 실패"));
		return;
	}

	UInventoryComponent* Inventory = Instigator->FindComponentByClass<UInventoryComponent>();
	if (!Inventory)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Retrieve|PickupGroup] %s에 InventoryComponent가 없음"),
			*Instigator->GetName());
		return;
	}

	int32 AddedCount = 0;
	for (const FRetrievePickupEntry& Entry : Items)
	{
		if (Entry.ItemId.IsNone() || !Entry.ItemCategoryTag.IsValid() || Entry.Quantity <= 0)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Retrieve|PickupGroup] 잘못된 엔트리 스킵: ItemId=%s Tag=%s Quantity=%d"),
				*Entry.ItemId.ToString(), *Entry.ItemCategoryTag.ToString(), Entry.Quantity);
			continue;
		}

		const bool bAdded = Inventory->AddItem(Entry.ItemId, Entry.ItemCategoryTag, Entry.Quantity);
		if (bAdded)
		{
			++AddedCount;
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("[Retrieve|PickupGroup] %s에 %d/%d개 엔트리 적용 완료"),
		*Instigator->GetName(), AddedCount, Items.Num());
}
