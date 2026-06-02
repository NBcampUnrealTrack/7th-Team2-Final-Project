#include "UI/HUD/RetrieveQuickSlotBarWidget.h"
#include "UI/HUD/RetrieveQuickSlotEntryWidget.h"
#include "Components/InventoryComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "GameFramework/Pawn.h"

void URetrieveQuickSlotBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ResolveInventoryComponent();
	ResolveSlotEntries();

	if (WBP_QuickSlotEntry_4)
	{
		WBP_QuickSlotEntry_4->InitSlot(UInventoryComponent::QuickSlotPrimaryKey, NAME_None, QuickSlotIconTable);
	}
	if (WBP_QuickSlotEntry_5)
	{
		WBP_QuickSlotEntry_5->InitSlot(UInventoryComponent::QuickSlotSecondaryKey, NAME_None, QuickSlotIconTable);
	}

	if (InventoryComp)
	{
		// 슬롯 변경 시 자동 갱신 (중복 바인딩 방지)
		InventoryComp->OnConsumableSlotChanged.RemoveDynamic(this, &URetrieveQuickSlotBarWidget::HandleConsumableSlotChanged);
		InventoryComp->OnConsumableSlotChanged.AddDynamic(this, &URetrieveQuickSlotBarWidget::HandleConsumableSlotChanged);

		// 아이템 획득/소모로 수량이 바뀌어도 갱신
		InventoryComp->OnInventoryChanged.RemoveDynamic(this, &URetrieveQuickSlotBarWidget::RefreshSlots);
		InventoryComp->OnInventoryChanged.AddDynamic(this, &URetrieveQuickSlotBarWidget::RefreshSlots);
	}

	RefreshSlots();
}

void URetrieveQuickSlotBarWidget::NativeDestruct()
{
	if (InventoryComp)
	{
		InventoryComp->OnConsumableSlotChanged.RemoveDynamic(this, &URetrieveQuickSlotBarWidget::HandleConsumableSlotChanged);
		InventoryComp->OnInventoryChanged.RemoveDynamic(this, &URetrieveQuickSlotBarWidget::RefreshSlots);
	}

	Super::NativeDestruct();
}

void URetrieveQuickSlotBarWidget::ResolveInventoryComponent()
{
	if (InventoryComp)
	{
		return;
	}

	if (APawn* OwningPawn = GetOwningPlayerPawn())
	{
		InventoryComp = OwningPawn->FindComponentByClass<UInventoryComponent>();
	}
}

void URetrieveQuickSlotBarWidget::ResolveSlotEntries()
{
	if (WBP_QuickSlotEntry_4 && WBP_QuickSlotEntry_5)
	{
		return;
	}

	TArray<UWidget*> Widgets;
	WidgetTree->GetAllWidgets(Widgets);

	for (UWidget* Widget : Widgets)
	{
		URetrieveQuickSlotEntryWidget* Entry = Cast<URetrieveQuickSlotEntryWidget>(Widget);
		if (!Entry)
		{
			continue;
		}

		if (!WBP_QuickSlotEntry_4)
		{
			WBP_QuickSlotEntry_4 = Entry;
		}
		else if (!WBP_QuickSlotEntry_5 && Entry != WBP_QuickSlotEntry_4)
		{
			WBP_QuickSlotEntry_5 = Entry;
			break;
		}
	}
}

void URetrieveQuickSlotBarWidget::HandleConsumableSlotChanged(int32 /*InSlotKey*/, FName /*ItemId*/)
{
	RefreshSlots();
}

void URetrieveQuickSlotBarWidget::RefreshSlots()
{
	// 폰이 늦게 준비되는 경우 대비 — 컴포넌트 재탐색
	if (!InventoryComp)
	{
		ResolveInventoryComponent();
	}

	if (!InventoryComp)
	{
		return;
	}

	ResolveSlotEntries();

	RefreshSlotEntry(WBP_QuickSlotEntry_4, UInventoryComponent::QuickSlotPrimaryKey);
	RefreshSlotEntry(WBP_QuickSlotEntry_5, UInventoryComponent::QuickSlotSecondaryKey);
}

void URetrieveQuickSlotBarWidget::RefreshSlotEntry(URetrieveQuickSlotEntryWidget* Entry, int32 SlotKey) const
{
	if (!Entry || !InventoryComp)
	{
		return;
	}

	const FName ItemId = InventoryComp->GetConsumableSlotItemId(SlotKey);
	Entry->InitSlot(SlotKey, ItemId, QuickSlotIconTable);
	Entry->RefreshCount(ItemId.IsNone() ? 0 : InventoryComp->GetItemCount(ItemId));
}
