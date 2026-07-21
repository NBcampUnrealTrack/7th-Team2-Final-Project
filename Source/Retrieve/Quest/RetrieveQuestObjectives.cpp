#include "Quest/RetrieveQuestObjectives.h"

#include "Components/Inventory/InventoryComponent.h"
#include "Components/World/RetrieveInteractionResponseComponent.h"
#include "Components/World/RetrieveMapIconComponent.h"
#include "Engine/World.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Kismet/GameplayStatics.h"
#include "Messaging/GameplayMessages/RetrieveGameplayMessageTypes.h"
#include "TimerManager.h"
#include "World/RetrieveQuestEncounter.h"

// ─────────────────────────────────────────────────────────────────────────────
// URetrieveQuestObjective (base)
// ─────────────────────────────────────────────────────────────────────────────
void URetrieveQuestObjective::ActivateObjective(ARetrieveQuestEncounter* InOwner)
{
	Owner = InOwner;
	if (bActive)
	{
		return;
	}
	bActive = true;
	OnActivate();
}

void URetrieveQuestObjective::DeactivateObjective()
{
	if (!bActive)
	{
		return;
	}
	bActive = false;
	OnDeactivate();
}

FText URetrieveQuestObjective::GetProgressText() const
{
	return Description;
}

void URetrieveQuestObjective::SerializeProgress(TArray<uint8>& Out) const
{
	Out.Reset();
	Out.Add(bComplete ? 1 : 0);
}

void URetrieveQuestObjective::RestoreProgress(const TArray<uint8>& In)
{
	bComplete = In.Num() > 0 && In[0] != 0;
}

void URetrieveQuestObjective::SetComplete(bool bNewComplete)
{
	if (bComplete == bNewComplete)
	{
		return;
	}
	bComplete = bNewComplete;

	// 완료 순간, 설정된 메시지가 있으면 아이템 획득 토스트로 시각 피드백을 준다
	// (예: 위치 도달 목표의 "탑을 청소했습니다"). 인벤토리 미경유 목표에 유용.
	if (bNewComplete && !CompletionToastMessage.IsEmptyOrWhitespace())
	{
		if (UWorld* World = GetOwnerWorld())
		{
			FRetrievePickupToastPayload Payload;
			Payload.Title = CompletionToastMessage;
			UGameplayMessageSubsystem::Get(World).BroadcastMessage(
				RetrieveGameplayTags::Channel_UI_PickupToast, Payload);
		}
	}

	BroadcastChanged();
}

UWorld* URetrieveQuestObjective::GetOwnerWorld() const
{
	return Owner.IsValid() ? Owner->GetWorld() : nullptr;
}

AActor* URetrieveQuestObjective::ResolveLinkedActor(FName Role) const
{
	return Owner.IsValid() ? Owner->GetLinkedActor(Role) : nullptr;
}

AActor* URetrieveQuestObjective::GetPlayerPawnSafe() const
{
	UWorld* World = GetOwnerWorld();
	return World ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
}

UInventoryComponent* URetrieveQuestObjective::GetPlayerInventory() const
{
	AActor* Pawn = GetPlayerPawnSafe();
	return Pawn ? Pawn->FindComponentByClass<UInventoryComponent>() : nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// UQuestObjective_ClearSpawnGroup
// ─────────────────────────────────────────────────────────────────────────────
void UQuestObjective_ClearSpawnGroup::OnActivate()
{
	if (bComplete || !Owner.IsValid())
	{
		return;
	}

	ClearedHandle = UGameplayMessageSubsystem::Get(Owner.Get()).RegisterListener<FSpawnGroupClearedPayload>(
		RetrieveGameplayTags::Channel_Enemy_SpawnGroupCleared,
		this, &UQuestObjective_ClearSpawnGroup::HandleSpawnGroupCleared);
}

void UQuestObjective_ClearSpawnGroup::OnDeactivate()
{
	if (ClearedHandle.IsValid() && Owner.IsValid())
	{
		UGameplayMessageSubsystem::Get(Owner.Get()).UnregisterListener(ClearedHandle);
	}
	ClearedHandle = FGameplayMessageListenerHandle();
}

void UQuestObjective_ClearSpawnGroup::HandleSpawnGroupCleared(
	FGameplayTag Channel, const FSpawnGroupClearedPayload& Payload)
{
	if (bComplete || !SpawnGroupId.IsValid() || Payload.SpawnGroupId != SpawnGroupId)
	{
		return;
	}
	SetComplete(true);
}

FText UQuestObjective_ClearSpawnGroup::GetProgressText() const
{
	return Description;
}

// ─────────────────────────────────────────────────────────────────────────────
// UQuestObjective_CollectWorldItem
// ─────────────────────────────────────────────────────────────────────────────
void UQuestObjective_CollectWorldItem::OnActivate()
{
	AActor* Target = ResolveLinkedActor(ItemActorRole);
	if (!Target)
	{
		return;
	}

	BoundResponse = Target->FindComponentByClass<URetrieveInteractionResponseComponent>();
	if (BoundResponse.IsValid())
	{
		BoundResponse->OnApplied.AddUniqueDynamic(this, &UQuestObjective_CollectWorldItem::HandleItemInteracted);
	}
	// 대상 액터의 표시/숨김은 인카운터가 SpawnRequest.VisiblePhases로 관리한다.
}

void UQuestObjective_CollectWorldItem::OnDeactivate()
{
	if (BoundResponse.IsValid())
	{
		BoundResponse->OnApplied.RemoveDynamic(this, &UQuestObjective_CollectWorldItem::HandleItemInteracted);
	}
	BoundResponse.Reset();
}

void UQuestObjective_CollectWorldItem::HandleItemInteracted(AActor* InteractionInstigator)
{
	if (bComplete || !IsValid(InteractionInstigator))
	{
		return;
	}

	// 회수 시각 피드백: 대상 액터의 맵 아이콘을 재활용해 아이템 획득 토스트를 발행한다.
	// (CollectWorldItem은 인벤토리를 경유하지 않아 OnItemAdded 토스트가 뜨지 않는다)
	if (UWorld* World = GetOwnerWorld())
	{
		FRetrievePickupToastPayload Payload;

		if (const AActor* Target = ResolveLinkedActor(ItemActorRole))
		{
			if (const URetrieveMapIconComponent* MapIcon =
				Target->FindComponentByClass<URetrieveMapIconComponent>())
			{
				if (MapIcon->bOverrideIcon)
				{
					Payload.Icon = MapIcon->OverrideTexture;
				}
				if (!MapIcon->MapLabel.IsEmptyOrWhitespace())
				{
					Payload.Title = MapIcon->MapLabel;
				}
			}
		}

		if (Payload.Title.IsEmptyOrWhitespace())
		{
			Payload.Title = !Description.IsEmptyOrWhitespace()
				? Description
				: NSLOCTEXT("Retrieve", "QuestItemPickup", "퀘스트 아이템 획득");
		}

		UGameplayMessageSubsystem::Get(World).BroadcastMessage(
			RetrieveGameplayTags::Channel_UI_PickupToast, Payload);
	}

	SetComplete(true);
}

FText UQuestObjective_CollectWorldItem::GetProgressText() const
{
	return Description;
}

// ─────────────────────────────────────────────────────────────────────────────
// UQuestObjective_ReachLocation
// ─────────────────────────────────────────────────────────────────────────────
void UQuestObjective_ReachLocation::OnActivate()
{
	if (bComplete)
	{
		return;
	}

	UWorld* World = GetOwnerWorld();
	if (!World)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		PollTimerHandle, FTimerDelegate::CreateUObject(this, &UQuestObjective_ReachLocation::PollDistance),
		0.25f, true);
}

void UQuestObjective_ReachLocation::OnDeactivate()
{
	if (UWorld* World = GetOwnerWorld())
	{
		World->GetTimerManager().ClearTimer(PollTimerHandle);
	}
}

void UQuestObjective_ReachLocation::PollDistance()
{
	if (bComplete)
	{
		return;
	}

	// 목적지 액터가 없으면 인카운터 자신의 위치를 목적지로 사용.
	const AActor* Destination = ResolveLinkedActor(DestinationRole);
	const AActor* Player = GetPlayerPawnSafe();
	const AActor* Reference = Destination ? Destination : Owner.Get();
	if (!Reference || !Player)
	{
		return;
	}

	const float DistSq = FVector::DistSquared(Reference->GetActorLocation(), Player->GetActorLocation());
	if (DistSq <= AcceptanceRadius * AcceptanceRadius)
	{
		if (UWorld* World = GetOwnerWorld())
		{
			World->GetTimerManager().ClearTimer(PollTimerHandle);
		}
		SetComplete(true);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// UQuestObjective_AcquireItem
// ─────────────────────────────────────────────────────────────────────────────
void UQuestObjective_AcquireItem::OnActivate()
{
	Recount();

	if (UInventoryComponent* Inventory = GetPlayerInventory())
	{
		BoundInventory = Inventory;
		Inventory->OnItemAdded.AddUniqueDynamic(this, &UQuestObjective_AcquireItem::HandleItemAdded);
	}
}

void UQuestObjective_AcquireItem::OnDeactivate()
{
	if (BoundInventory.IsValid())
	{
		BoundInventory->OnItemAdded.RemoveDynamic(this, &UQuestObjective_AcquireItem::HandleItemAdded);
	}
	BoundInventory.Reset();
}

void UQuestObjective_AcquireItem::HandleItemAdded(FName InItemId, FGameplayTag Category, int32 Quantity)
{
	if (InItemId != ItemId)
	{
		return;
	}
	Recount();
}

void UQuestObjective_AcquireItem::Recount()
{
	int32 NewCount = 0;
	if (const UInventoryComponent* Inventory = GetPlayerInventory())
	{
		NewCount = Inventory->GetItemCount(ItemId);
	}

	const int32 Clamped = FMath::Min(NewCount, RequiredCount);
	const bool bChanged = Clamped != CurrentCount;
	CurrentCount = Clamped;

	const bool bNowComplete = CurrentCount >= RequiredCount;
	if (bNowComplete != bComplete)
	{
		SetComplete(bNowComplete);
	}
	else if (bChanged)
	{
		BroadcastChanged();
	}
}

FText UQuestObjective_AcquireItem::GetProgressText() const
{
	return FText::Format(
		NSLOCTEXT("RetrieveQuest", "AcquireItemProgress", "{0} ({1}/{2})"),
		Description, FText::AsNumber(CurrentCount), FText::AsNumber(RequiredCount));
}

bool UQuestObjective_AcquireItem::CanTurnIn(AActor* Player) const
{
	if (!bConsumeOnTurnIn)
	{
		return true;
	}
	const UInventoryComponent* Inventory = Player ? Player->FindComponentByClass<UInventoryComponent>() : nullptr;
	return Inventory && Inventory->HasItem(ItemId, RequiredCount);
}

void UQuestObjective_AcquireItem::OnTurnIn(AActor* Player)
{
	if (!bConsumeOnTurnIn)
	{
		return;
	}
	if (UInventoryComponent* Inventory = Player ? Player->FindComponentByClass<UInventoryComponent>() : nullptr)
	{
		Inventory->RemoveItem(ItemId, ItemCategoryTag, RequiredCount);
	}
}
