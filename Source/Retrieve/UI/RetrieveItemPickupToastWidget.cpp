#include "UI/RetrieveItemPickupToastWidget.h"

#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Engine/DataTable.h"
#include "GameplayTags/RetrieveGameplayTags.h"

void URetrieveItemPickupToastWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!ToastVFXTarget)
	{
		ToastVFXTarget = GetWidgetFromName(TEXT("ToastBG"));
	}
	if (!DefaultVFXTarget && ToastVFXTarget)
	{
		DefaultVFXTarget = ToastVFXTarget;
	}
}

void URetrieveItemPickupToastWidget::InitToast(FName ItemId, FGameplayTag ItemCategoryTag, int32 Quantity)
{
	UE_LOG(LogTemp, Warning, TEXT("[ToastWidget] InitToast CALLED: ItemId=%s Tag=%s Qty=%d | ItemNameText=%s QuantityText=%s"),
		*ItemId.ToString(),
		*ItemCategoryTag.ToString(),
		Quantity,
		ItemNameText ? TEXT("VALID") : TEXT("NULL"),
		QuantityText ? TEXT("VALID") : TEXT("NULL"));

	const FText DisplayName = LookupItemDisplayName(ItemId, ItemCategoryTag);

	UE_LOG(LogTemp, Warning, TEXT("[ToastWidget] DisplayName='%s'"), *DisplayName.ToString());

	if (ItemNameText)
	{
		ItemNameText->SetText(DisplayName);
	}

	if (QuantityText)
	{
		QuantityText->SetText(FText::FromString(FString::Printf(TEXT("×%d"), Quantity)));
	}

	PlayUIVFXOnWidget(
		RetrieveGameplayTags::UI_VFX_Icon_ItemAdded,
		ToastVFXTarget ? ToastVFXTarget.Get() : ResolveDefaultVFXTarget());
}

void URetrieveItemPickupToastWidget::SetToastSlotIndex(int32 SlotIndex, float SlotStartY, float SlotHeight)
{
	UWidget* BG = GetWidgetFromName(TEXT("ToastBG"));
	if (!BG)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ToastWidget] SetToastSlotIndex: ToastBG not found"));
		return;
	}

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(BG->Slot))
	{
		// center_right 앵커: Y=0이 화면 수직 중앙, 음수 Y = 위쪽
		// alignment(1, 0) = 위젯 오른쪽 상단 모서리가 앵커 포인트에 맞춰짐
		CanvasSlot->SetAnchors(FAnchors(1.f, 0.5f, 1.f, 0.5f));
		CanvasSlot->SetAlignment(FVector2D(1.f, 0.f));
		CanvasSlot->SetPosition(FVector2D(-16.f, SlotStartY + SlotIndex * SlotHeight));
	}
}

FText URetrieveItemPickupToastWidget::LookupItemDisplayName(FName ItemId, FGameplayTag ItemCategoryTag)
{
	// LoadObject에 전체 경로(패키지.오브젝트) 사용 — 경로만 쓰면 찾지 못할 수 있음
	if (ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Weapon))
	{
		if (UDataTable* DT = LoadObject<UDataTable>(
			nullptr, TEXT("/Game/Retrieve/Data/Items/DT_WeaponData.DT_WeaponData")))
		{
			if (const FRetrieveWeaponDataRow* Row =
				DT->FindRow<FRetrieveWeaponDataRow>(ItemId, TEXT("InitToast"), false))
			{
				return Row->DisplayName;
			}
			UE_LOG(LogTemp, Warning, TEXT("[ToastWidget] Weapon row '%s' NOT FOUND"), *ItemId.ToString());
		}
		else { UE_LOG(LogTemp, Warning, TEXT("[ToastWidget] DT_WeaponData load FAILED")); }
	}
	else if (ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Consumable))
	{
		if (UDataTable* DT = LoadObject<UDataTable>(
			nullptr, TEXT("/Game/Retrieve/Data/Items/DT_ConsumableItem.DT_ConsumableItem")))
		{
			if (const FRetrieveConsumableItemRow* Row =
				DT->FindRow<FRetrieveConsumableItemRow>(ItemId, TEXT("InitToast"), false))
			{
				return Row->DisplayName;
			}
			UE_LOG(LogTemp, Warning, TEXT("[ToastWidget] Consumable row '%s' NOT FOUND"), *ItemId.ToString());
		}
		else { UE_LOG(LogTemp, Warning, TEXT("[ToastWidget] DT_ConsumableItem load FAILED")); }
	}
	else if (ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Material))
	{
		if (UDataTable* DT = LoadObject<UDataTable>(
			nullptr, TEXT("/Game/Retrieve/Data/Items/DT_MaterialItem.DT_MaterialItem")))
		{
			if (const FRetrieveMaterialItemRow* Row =
				DT->FindRow<FRetrieveMaterialItemRow>(ItemId, TEXT("InitToast"), false))
			{
				return Row->DisplayName;
			}
			UE_LOG(LogTemp, Warning, TEXT("[ToastWidget] Material row '%s' NOT FOUND"), *ItemId.ToString());
		}
		else { UE_LOG(LogTemp, Warning, TEXT("[ToastWidget] DT_MaterialItem load FAILED")); }
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[ToastWidget] Tag '%s' matched no category, trying all tables"), *ItemCategoryTag.ToString());

		if (UDataTable* WDT = LoadObject<UDataTable>(
			nullptr, TEXT("/Game/Retrieve/Data/Items/DT_WeaponData.DT_WeaponData")))
		{
			if (const FRetrieveWeaponDataRow* Row =
				WDT->FindRow<FRetrieveWeaponDataRow>(ItemId, TEXT("InitToast"), false))
				return Row->DisplayName;
		}
		if (UDataTable* CDT = LoadObject<UDataTable>(
			nullptr, TEXT("/Game/Retrieve/Data/Items/DT_ConsumableItem.DT_ConsumableItem")))
		{
			if (const FRetrieveConsumableItemRow* Row =
				CDT->FindRow<FRetrieveConsumableItemRow>(ItemId, TEXT("InitToast"), false))
				return Row->DisplayName;
		}
		if (UDataTable* MDT = LoadObject<UDataTable>(
			nullptr, TEXT("/Game/Retrieve/Data/Items/DT_MaterialItem.DT_MaterialItem")))
		{
			if (const FRetrieveMaterialItemRow* Row =
				MDT->FindRow<FRetrieveMaterialItemRow>(ItemId, TEXT("InitToast"), false))
				return Row->DisplayName;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[ToastWidget] LookupItemDisplayName FALLBACK for '%s'"), *ItemId.ToString());
	return FText::FromName(ItemId);
}
