#include "UI/Inventory/InventoryPanelWidget.h"
#include "UI/HUD/RetrieveQuickSlotWheelWidget.h"

#include "UI/RetrieveItemDescriptionHelper.h"
#include "AbilitySystem/Attributes/CombatAttributeSet.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Inventory/InventoryComponent.h"
#include "Character/RetrievePawnData.h"
#include "Components/Pawn/RetrievePawnExtensionComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Player/WeaponComponent.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "GameFramework/Pawn.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Engine/Texture2D.h"
#include "UObject/UnrealType.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
void SetTooltipText(UUserWidget* TooltipWidget, const TCHAR* WidgetName, const FString& Value, bool bCollapseWhenEmpty = false)
{
	if (!TooltipWidget)
	{
		return;
	}

	if (UTextBlock* TextBlock = Cast<UTextBlock>(TooltipWidget->GetWidgetFromName(WidgetName)))
	{
		TextBlock->SetText(FText::FromString(Value));
		TextBlock->SetAutoWrapText(true);
		TextBlock->SetVisibility(bCollapseWhenEmpty && Value.IsEmpty()
			? ESlateVisibility::Collapsed
			: ESlateVisibility::Visible);
	}
}

void SetTooltipImage(UUserWidget* TooltipWidget, const TCHAR* WidgetName, UTexture2D* Texture)
{
	if (!TooltipWidget || !Texture)
	{
		return;
	}

	if (UImage* Image = Cast<UImage>(TooltipWidget->GetWidgetFromName(WidgetName)))
	{
		Image->SetBrushFromTexture(Texture, false);
		Image->SetColorAndOpacity(FLinearColor::White);
	}
}

void SetTooltipWidgetVisible(UUserWidget* TooltipWidget, const TCHAR* WidgetName, bool bVisible)
{
	if (TooltipWidget)
	{
		if (UWidget* Widget = TooltipWidget->GetWidgetFromName(WidgetName))
		{
			Widget->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	}
}
}

UInventoryPanelWidget::UInventoryPanelWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WeaponTabCategoryTag = RetrieveGameplayTags::Item_Weapon;
	ConsumableTabCategoryTag = RetrieveGameplayTags::Item_Consumable;
	MaterialTabCategoryTag = RetrieveGameplayTags::Item_Material;
	ArmorTabCategoryTag = RetrieveGameplayTags::Item_Armor;
	CurrentCategoryTag = WeaponTabCategoryTag;
	CurrentSortMode = EInventorySortMode::AttackPowerDesc;
}

void UInventoryPanelWidget::NativeOnInitialized()
{
	// 이 WBP는 한때 Monolith(에디터 전용 플러그인)의 GAS 어트리뷰트 바인딩 확장을 컴파일된
	// 클래스에 포함하고 있었다. 그 확장 클래스는 Shipping에 로드되지 않는 Editor 전용
	// 모듈이라, Shipping에서 이 클래스를 로드하면 UUserWidget::Extensions 배열에 null
	// 항목이 남는다. Super::NativeOnInitialized()는 각 Extension에 대해 check() 없이
	// (Shipping에서는 DO_CHECK=0이라 스트립됨) 바로 Initialize()를 호출해 null 역참조로
	// 크래시한다. private 프로퍼티라 리플렉션으로 직접 정리한다.
	if (FArrayProperty* ExtensionsProp = FindFProperty<FArrayProperty>(UUserWidget::StaticClass(), TEXT("Extensions")))
	{
		if (FObjectProperty* InnerObjectProp = CastField<FObjectProperty>(ExtensionsProp->Inner))
		{
			FScriptArrayHelper ArrayHelper(ExtensionsProp, ExtensionsProp->ContainerPtrToValuePtr<void>(this));
			for (int32 Index = ArrayHelper.Num() - 1; Index >= 0; --Index)
			{
				if (!InnerObjectProp->GetObjectPropertyValue(ArrayHelper.GetRawPtr(Index)))
				{
					ArrayHelper.RemoveValues(Index, 1);
				}
			}
		}
	}

	Super::NativeOnInitialized();
}

void UInventoryPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	InitDefaultTags();
	ResolveDefaultTooltipWidgetClasses();
	InitOwnerComponents();
	BindInventoryEvents();
	BindButtonEvents();
	if (Button_SortAttackPower)
	{
		Button_SortAttackPower->SetVisibility(ESlateVisibility::Visible);
	}
	if (Button_SortDefense)
	{
		Button_SortDefense->SetVisibility(ESlateVisibility::Collapsed);
	}
	ShowWeaponSwapConfirm(false);
	HideQuickSlotAssignDialog();
	ShowQuickSlotReplaceConfirm(false);
	MarkInventoryTooltipsDirty();
	RefreshInventoryView(false);
	RefreshWeaponComparisonText();
	UpdateQuickSlotPanel();
	UpdateQuickSlotActionButtons();
	RefreshInventoryGridLayout();
	RefreshSlotIcons();
	RefreshStatDisplay();
	BindEquipLockTagEvents();
}

void UInventoryPanelWidget::NativeDestruct()
{
	UnbindEquipLockTagEvents();
	Super::NativeDestruct();
}

void UInventoryPanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshInventoryGridLayout();
	SuppressBlueprintManagedTooltip();

	// 그리드 슬롯 툴팁(Compare/Detail)이 슬롯 밖으로 마우스가 나간 뒤에도 커서를 따라다니며
	// 남아있는 경우에 대한 안전장치. 그리드 안의 슬롯이 하나도 호버되지 않으면 강제로 닫는다.
	if (UniformGrid_ItemList && !IsWidgetOrDescendantHovered(UniformGrid_ItemList))
	{
		FSlateApplication::Get().CloseToolTip();
	}
}

void UInventoryPanelWidget::InitializeInventoryPanel(UInventoryComponent* InInventoryComponent, UWeaponComponent* InWeaponComponent)
{
	InventoryComponent = InInventoryComponent;
	WeaponComponent = InWeaponComponent;
	InitDefaultTags();
	ResolveDefaultTooltipWidgetClasses();
	BindInventoryEvents();
	MarkInventoryTooltipsDirty();
	RefreshInventoryView(false);
	RefreshWeaponComparisonText();
	UpdateQuickSlotPanel();
	UpdateQuickSlotActionButtons();
	BindEquipLockTagEvents();
	OnEquipmentChanged.Broadcast();
}

void UInventoryPanelWidget::OpenTab(int32 TabIndex)
{
	// 탭별 기본 정렬을 적용해 다른 카테고리의 정렬 기준이 남지 않도록 한다.
	ActiveTabIndex = TabIndex;
	MarkInventoryTooltipsDirty();
	OnTabSwitchRequested.Broadcast(TabIndex);

	if (TabIndex == 0)
	{
		CurrentCategoryTag = WeaponTabCategoryTag;
		CurrentSortMode = EInventorySortMode::AttackPowerDesc;
		RefreshInventoryView(true);
	}
	else if (TabIndex == 1)
	{
		CurrentCategoryTag = ConsumableTabCategoryTag;
		CurrentSortMode = EInventorySortMode::None;
		RefreshInventoryView(true);
	}
	else if (TabIndex == 2)
	{
		CurrentCategoryTag = MaterialTabCategoryTag;
		CurrentSortMode = EInventorySortMode::None;
		RefreshInventoryView(true);
	}
	else if (TabIndex == 3)
	{
		CurrentCategoryTag = ArmorTabCategoryTag;
		CurrentSortMode = EInventorySortMode::DefenseDesc;
		RefreshInventoryView(true);
	}

	if (Button_SortAttackPower)
	{
		Button_SortAttackPower->SetVisibility(GetSortAttackPowerButtonVisibility());
	}
	if (Button_SortDefense)
	{
		Button_SortDefense->SetVisibility(GetSortDefenseButtonVisibility());
	}

	PlayUIVFXOnWidget(
		RetrieveGameplayTags::UI_VFX_Tab_Switch,
		UniformGrid_ItemList ? Cast<UWidget>(UniformGrid_ItemList.Get()) : GetRootWidget());
}

void UInventoryPanelWidget::RefreshInventoryList()
{
	// 목록 갱신 시 기존 선택 해제. 상세 패널은 새 선택 이벤트로 갱신
	RefreshInventoryView(true);
}

void UInventoryPanelWidget::SelectItem(FName ItemId, FGameplayTag ItemCategoryTag, int32 SlotInstanceId)
{
	UE_LOG(LogTemp, Log, TEXT("[SelectItem] ItemId=%s CategoryTag=%s SlotInstanceId=%d"), *ItemId.ToString(), *ItemCategoryTag.ToString(), SlotInstanceId);
	if (!ItemId.IsNone()
		&& SelectedItemId == ItemId
		&& SelectedItemCategoryTag == ItemCategoryTag
		&& SelectedSlotInstanceId == SlotInstanceId)
	{
		// 클릭 BP가 선택과 활성화를 별도 단계로 처리한다. 여기서 다시 활성화하면
		// 한 번의 재클릭으로 장착/해제 함수가 중복 호출되어 더블클릭이 불안정해진다.
		return;
	}

	SelectedItemId = ItemId;
	SelectedItemCategoryTag = ItemCategoryTag;
	SelectedSlotInstanceId = SlotInstanceId;
	ShowWeaponSwapConfirm(false);
	HideQuickSlotAssignDialog();
	RefreshWeaponComparisonText();
	RefreshSelectedDetailState();
	UpdateQuickSlotActionButtons();
	OnSelectedItemChanged.Broadcast(SelectedItemId, SelectedItemCategoryTag);
	RefreshSelectedMaterialDetails();
	RefreshSelectedArmorDetails();
	UpdateEquipActionButtons();
}

bool UInventoryPanelWidget::ActivateSelectedItem()
{
	if (SelectedItemId.IsNone())
	{
		return false;
	}

	if (!CanProcessSelectedItemActivation())
	{
		UE_LOG(LogTemp, Verbose, TEXT("[ActivateSelectedItem] duplicate activation ignored: ItemId=%s CategoryTag=%s"),
			*SelectedItemId.ToString(),
			*SelectedItemCategoryTag.ToString());
		return false;
	}

	TGuardValue<bool> ActivationGuard(bBypassSelectedItemActivationGuard, true);

	if (IsWeaponCategory(SelectedItemCategoryTag))
	{
		return IsSelectedWeaponEquipped()
			? UnequipCurrentWeapon()
			: EquipSelectedWeapon();
	}

	if (IsConsumableCategory(SelectedItemCategoryTag))
	{
		return UseSelectedConsumable();
	}

	if (IsArmorCategory(SelectedItemCategoryTag))
	{
		return IsSelectedArmorEquipped()
			? UnequipSelectedArmor()
			: EquipSelectedArmor();
	}

	return false;
}

bool UInventoryPanelWidget::SelectAndActivateItem(FName ItemId, FGameplayTag ItemCategoryTag, int32 SlotInstanceId)
{
	if (ItemId.IsNone())
	{
		SelectItem(ItemId, ItemCategoryTag, SlotInstanceId);
		LastGridSlotClickTime = -1.0;
		return false;
	}

	const double CurrentTime = FPlatformTime::Seconds();
	const bool bClickedAlreadySelectedItem =
		SelectedItemId == ItemId
		&& SelectedItemCategoryTag == ItemCategoryTag
		&& SelectedSlotInstanceId == SlotInstanceId;

	// 이전 클릭이 "같은 아이템"이면서 FastDoubleClickThresholdSeconds 안에 들어온 경우에만
	// 진짜 더블클릭으로 인정해 장착/해제를 실행한다. 느리게 두 번 클릭한 경우는 두 번째 클릭도
	// 그냥 선택 클릭으로만 처리해서 더블클릭 판정이 나지 않게 한다.
	const bool bIsFastDoubleClick =
		bClickedAlreadySelectedItem
		&& LastGridSlotClickTime >= 0.0
		&& (CurrentTime - LastGridSlotClickTime) <= FastDoubleClickThresholdSeconds;

	if (bIsFastDoubleClick)
	{
		// 이번 더블클릭을 소비한다. 바로 다음에 이어지는 느린 클릭이 이 클릭과 짝지어져
		// 또 다른 더블클릭으로 판정되는 것을 막는다.
		LastGridSlotClickTime = -1.0;
		return ActivateSelectedItem();
	}

	SelectItem(ItemId, ItemCategoryTag, SlotInstanceId);
	LastGridSlotClickTime = CurrentTime;
	return false;
}

bool UInventoryPanelWidget::EquipSelectedWeapon()
{
	if (!IsWeaponCategory(SelectedItemCategoryTag))
	{
		return ActivateSelectedItem();
	}

	if (!bBypassSelectedItemActivationGuard && !CanProcessSelectedItemActivation())
	{
		return false;
	}

	if (!CanEquipSelectedWeapon())
	{
		return false;
	}

	// 실제 장착 가능 여부는 InventoryComponent에서 재검사
	if (!bBypassWeaponSwapConfirm && ShouldConfirmWeaponSwap())
	{
		ShowWeaponSwapConfirm(true);
		return false;
	}

	ShowWeaponSwapConfirm(false);

	const bool bEquipped = InventoryComponent->RequestEquipWeapon(SelectedItemId, SelectedSlotInstanceId);
	if (bEquipped)
	{
		// RequestEquipWeapon이 OnEquippedWeaponChanged/OnInventoryChanged를 발동시켜
		// HandleEquippedWeaponChanged·HandleInventoryChanged에서 목록/버튼/이벤트가 모두 갱신된다.
		// 여기서 같은 Broadcast를 다시 호출하면 목록이 한 번 더 재생성되어 더블클릭 중인
		// 슬롯 위젯이 파괴되므로(더블클릭 먹통 원인) 중복 호출하지 않는다.
		MarkInventoryTooltipsDirty();
		RefreshWeaponComparisonText();
		UpdateEquipActionButtons();
		PlayContextUISound(RetrieveGameplayTags::UI_Sound_Inventory_Equip, ERetrieveUISoundEvent::Release);
	}
	return bEquipped;
}

bool UInventoryPanelWidget::UnequipCurrentWeapon()
{
	if (!IsWeaponCategory(SelectedItemCategoryTag))
	{
		return ActivateSelectedItem();
	}

	if (!bBypassSelectedItemActivationGuard && !CanProcessSelectedItemActivation())
	{
		return false;
	}

	if (!CanUnequipCurrentWeapon())
	{
		return false;
	}

	const bool bUnequipped = InventoryComponent->RequestUnequipWeapon();
	if (bUnequipped)
	{
		// 장착과 동일: RequestUnequipWeapon이 델리게이트로 목록/버튼/이벤트를 갱신하므로
		// 여기서 중복 Broadcast하지 않는다(목록 중복 재생성 방지).
		MarkInventoryTooltipsDirty();
		ShowWeaponSwapConfirm(false);
		RefreshWeaponComparisonText();
		UpdateEquipActionButtons();
		OnWeaponPreviewClearNeeded();
		PlayContextUISound(RetrieveGameplayTags::UI_Sound_Inventory_Unequip, ERetrieveUISoundEvent::Release);
	}
	return bUnequipped;
}

bool UInventoryPanelWidget::EquipSelectedArmor()
{
	UE_LOG(LogTemp, Log, TEXT("[EquipArmor] called — ItemId=%s Tag=%s"), *SelectedItemId.ToString(), *SelectedItemCategoryTag.ToString());
	if (!bBypassSelectedItemActivationGuard && !CanProcessSelectedItemActivation())
	{
		return false;
	}

	if (!CanEquipSelectedArmor())
	{
		UE_LOG(LogTemp, Warning, TEXT("[EquipArmor] CanEquipSelectedArmor returned false, aborting"));
		return false;
	}

	FRetrieveArmorDataRow ArmorData;
	if (!GetSelectedArmorData(ArmorData))
	{
		UE_LOG(LogTemp, Warning, TEXT("[EquipArmor] GetSelectedArmorData failed — ArmorDataTable row not found for %s"), *SelectedItemId.ToString());
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("[EquipArmor] requesting equip — SlotTag=%s ItemId=%s"), *ArmorData.EquipmentSlotTag.ToString(), *SelectedItemId.ToString());
	const bool bEquipped = InventoryComponent->RequestEquipArmor(ArmorData.EquipmentSlotTag, SelectedItemId, SelectedSlotInstanceId);
	UE_LOG(LogTemp, Log, TEXT("[EquipArmor] RequestEquipArmor result=%d"), bEquipped ? 1 : 0);
	if (bEquipped)
	{
		MarkInventoryTooltipsDirty();
		RefreshWeaponComparisonText();
		RefreshSelectedDetailState();
		RefreshSelectedArmorDetails();
		OnInventoryListChanged.Broadcast();
		UpdateEquipActionButtons();
		PlayContextUISound(RetrieveGameplayTags::UI_Sound_Inventory_Equip, ERetrieveUISoundEvent::Release);
	}
	return bEquipped;
}

bool UInventoryPanelWidget::UnequipSelectedArmor()
{
	if (!bBypassSelectedItemActivationGuard && !CanProcessSelectedItemActivation())
	{
		return false;
	}

	if (!CanUnequipSelectedArmor())
	{
		return false;
	}

	FRetrieveArmorDataRow ArmorData;
	if (!GetSelectedArmorData(ArmorData))
	{
		return false;
	}

	const bool bUnequipped = InventoryComponent->RequestUnequipArmor(ArmorData.EquipmentSlotTag);
	if (bUnequipped)
	{
		MarkInventoryTooltipsDirty();
		RefreshWeaponComparisonText();
		RefreshSelectedDetailState();
		RefreshSelectedArmorDetails();
		OnInventoryListChanged.Broadcast();
		UpdateEquipActionButtons();
		PlayContextUISound(RetrieveGameplayTags::UI_Sound_Inventory_Unequip, ERetrieveUISoundEvent::Release);
	}
	return bUnequipped;
}

bool UInventoryPanelWidget::UseSelectedConsumable()
{
	if (!bBypassSelectedItemActivationGuard && !CanProcessSelectedItemActivation())
	{
		return false;
	}

	if (!CanUseSelectedConsumable())
	{
		return false;
	}

	const FName UsedItemId = SelectedItemId;
	const FGameplayTag UsedCategoryTag = SelectedItemCategoryTag;
	const bool bUsed = InventoryComponent->UseConsumableItem(UsedItemId);
	if (bUsed)
	{
		MarkInventoryTooltipsDirty();
		if (InventoryComponent->GetItemCount(UsedItemId) <= 0)
		{
			ClearSelection();
		}
		else
		{
			RefreshWeaponComparisonText();
			OnSelectedItemChanged.Broadcast(UsedItemId, UsedCategoryTag);
		}
		OnInventoryListChanged.Broadcast();
		PlayContextUISound(RetrieveGameplayTags::UI_Sound_Inventory_UseConsumable, ERetrieveUISoundEvent::Release);
	}
	return bUsed;
}

void UInventoryPanelWidget::ShowQuickSlotAssignDialog()
{
	if (!CanAssignSelectedConsumableToQuickSlot())
	{
		return;
	}

	if (Border_QuickSlotAssignDialog)
	{
		Border_QuickSlotAssignDialog->SetVisibility(ESlateVisibility::Visible);
	}
}

void UInventoryPanelWidget::HideQuickSlotAssignDialog()
{
	if (Border_QuickSlotAssignDialog)
	{
		Border_QuickSlotAssignDialog->SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool UInventoryPanelWidget::AssignSelectedConsumableToSlot(int32 SlotKey)
{
	return AssignSelectedItemToQuickSlot(SlotKey, true);
}

bool UInventoryPanelWidget::UnassignSelectedConsumableSlot()
{
	return UnassignSelectedQuickSlot();
}

TArray<FRetrieveItemStack> UInventoryPanelWidget::GetCurrentItems() const
{
	return InventoryComponent
		? InventoryComponent->GetItemsByCategory(CurrentCategoryTag)
		: TArray<FRetrieveItemStack>();
}

bool UInventoryPanelWidget::IsSelectedWeaponEquipped() const
{
	return IsWeaponCategory(SelectedItemCategoryTag)
		&& IsWeaponItemEquipped(SelectedItemId, SelectedSlotInstanceId);
}

bool UInventoryPanelWidget::IsItemSelected(FName ItemId, int32 SlotInstanceId) const
{
	return !ItemId.IsNone()
		&& SelectedItemId == ItemId
		&& SelectedSlotInstanceId == SlotInstanceId;
}

bool UInventoryPanelWidget::IsWeaponItemEquipped(FName WeaponItemId, int32 SlotInstanceId) const
{
	if (!InventoryComponent
		|| WeaponItemId.IsNone()
		|| InventoryComponent->GetEquippedWeaponId() != WeaponItemId)
	{
		return false;
	}
	return InventoryComponent->GetEquippedWeaponSlotInstanceId() == SlotInstanceId;
}

bool UInventoryPanelWidget::IsArmorItemEquipped(FName ArmorItemId, int32 SlotInstanceId) const
{
	if (!InventoryComponent || ArmorItemId.IsNone() || !ArmorDataTable) return false;
	const FRetrieveArmorDataRow* Row = ArmorDataTable->FindRow<FRetrieveArmorDataRow>(
		ArmorItemId, TEXT("UInventoryPanelWidget::IsArmorItemEquipped"));
	if (!Row) return false;
	if (InventoryComponent->GetEquippedArmorId(Row->EquipmentSlotTag) != ArmorItemId)
	{
		return false;
	}
	for (const FRetrieveEquippedArmorEntry& EquippedSlot : InventoryComponent->GetEquippedArmorSlots())
	{
		if (EquippedSlot.EquipmentSlotTag == Row->EquipmentSlotTag)
		{
			return EquippedSlot.SlotInstanceId == SlotInstanceId;
		}
	}
	return false;
}

bool UInventoryPanelWidget::CanEquipSelectedWeapon() const
{
	return InventoryComponent
		&& IsWeaponCategory(SelectedItemCategoryTag)
		&& !SelectedItemId.IsNone()
		&& InventoryComponent->CanChangeEquipment()
		&& !IsSelectedWeaponEquipped();
}

bool UInventoryPanelWidget::CanUnequipCurrentWeapon() const
{
	return InventoryComponent
		&& InventoryComponent->CanChangeEquipment()
		&& !InventoryComponent->GetEquippedWeaponId().IsNone();
}

bool UInventoryPanelWidget::CanUseSelectedConsumable() const
{
	return InventoryComponent
		&& IsConsumableCategory(SelectedItemCategoryTag)
		&& !SelectedItemId.IsNone()
		&& InventoryComponent->GetItemCount(SelectedItemId) > 0;
}

bool UInventoryPanelWidget::CanAssignSelectedItemToQuickSlot() const
{
	if (!InventoryComponent || SelectedItemId.IsNone())
	{
		return false;
	}

	if (IsWeaponCategory(SelectedItemCategoryTag) || IsConsumableCategory(SelectedItemCategoryTag))
	{
		return InventoryComponent->GetItemCount(SelectedItemId) > 0;
	}

	return false;
}

bool UInventoryPanelWidget::IsSelectedItemAssignedToQuickSlot() const
{
	return GetSelectedQuickSlotKey() != INDEX_NONE;
}

int32 UInventoryPanelWidget::GetSelectedQuickSlotKey() const
{
	return InventoryComponent
		? InventoryComponent->GetAssignedQuickSlotKey(SelectedItemId, SelectedItemCategoryTag)
		: INDEX_NONE;
}

bool UInventoryPanelWidget::CanAssignSelectedConsumableToQuickSlot() const
{
	return CanAssignSelectedItemToQuickSlot();
}

bool UInventoryPanelWidget::IsSelectedConsumableAssignedToQuickSlot() const
{
	return IsSelectedItemAssignedToQuickSlot();
}

int32 UInventoryPanelWidget::GetSelectedConsumableSlotKey() const
{
	return GetSelectedQuickSlotKey();
}

FName UInventoryPanelWidget::GetQuickSlotItemId(int32 SlotKey) const
{
	return InventoryComponent ? InventoryComponent->GetConsumableSlotItemId(SlotKey) : NAME_None;
}

FText UInventoryPanelWidget::GetQuickSlotDisplayText(int32 SlotKey) const
{
	const FName SlotItemId = GetQuickSlotItemId(SlotKey);
	if (SlotItemId.IsNone())
	{
		return FText::FromString(FString::Printf(TEXT("%d\nEmpty"), SlotKey));
	}

	FString ItemName = SlotItemId.ToString();
	FRetrieveConsumableItemRow ConsumableData;
	if (ConsumableItemTable)
	{
		if (const FRetrieveConsumableItemRow* Row = ConsumableItemTable->FindRow<FRetrieveConsumableItemRow>(SlotItemId, TEXT("UInventoryPanelWidget::GetQuickSlotDisplayText")))
		{
			if (!Row->DisplayName.IsEmpty())
			{
				ItemName = Row->DisplayName.ToString();
			}
		}
	}

	const int32 Count = InventoryComponent ? InventoryComponent->GetItemCount(SlotItemId) : 0;
	return FText::FromString(FString::Printf(TEXT("%d\n%s x%d"), SlotKey, *ItemName, Count));
}

bool UInventoryPanelWidget::GetSelectedWeaponData(FRetrieveWeaponDataRow& OutWeaponData) const
{
	if (!WeaponDataTable || SelectedItemId.IsNone() || !IsWeaponCategory(SelectedItemCategoryTag))
	{
		return false;
	}

	if (const FRetrieveWeaponDataRow* Row = WeaponDataTable->FindRow<FRetrieveWeaponDataRow>(SelectedItemId, TEXT("UInventoryPanelWidget::GetSelectedWeaponData")))
	{
		OutWeaponData = *Row;
		return true;
	}
	return false;
}

bool UInventoryPanelWidget::GetCurrentWeaponData(FRetrieveWeaponDataRow& OutWeaponData) const
{
	if (!WeaponDataTable || !InventoryComponent)
	{
		return false;
	}

	const FName EquippedWeaponId = InventoryComponent->GetEquippedWeaponId();
	if (EquippedWeaponId.IsNone())
	{
		return false;
	}

	if (const FRetrieveWeaponDataRow* Row = WeaponDataTable->FindRow<FRetrieveWeaponDataRow>(EquippedWeaponId, TEXT("UInventoryPanelWidget::GetCurrentWeaponData")))
	{
		OutWeaponData = *Row;
		return true;
	}
	return false;
}

bool UInventoryPanelWidget::GetSelectedConsumableData(FRetrieveConsumableItemRow& OutConsumableData) const
{
	if (!ConsumableItemTable || SelectedItemId.IsNone() || !IsConsumableCategory(SelectedItemCategoryTag))
	{
		return false;
	}

	if (const FRetrieveConsumableItemRow* Row = ConsumableItemTable->FindRow<FRetrieveConsumableItemRow>(SelectedItemId, TEXT("UInventoryPanelWidget::GetSelectedConsumableData")))
	{
		OutConsumableData = *Row;
		return true;
	}
	return false;
}

bool UInventoryPanelWidget::GetSelectedMaterialData(FRetrieveMaterialItemRow& OutMaterialData) const
{
	UDataTable* Table = ResolveMaterialItemTable();
	if (!Table || SelectedItemId.IsNone() || !IsMaterialCategory(SelectedItemCategoryTag))
	{
		return false;
	}

	if (const FRetrieveMaterialItemRow* Row = Table->FindRow<FRetrieveMaterialItemRow>(SelectedItemId, TEXT("UInventoryPanelWidget::GetSelectedMaterialData")))
	{
		OutMaterialData = *Row;
		return true;
	}
	return false;
}

bool UInventoryPanelWidget::GetSelectedArmorData(FRetrieveArmorDataRow& OutArmorData) const
{
	if (!ArmorDataTable || SelectedItemId.IsNone() || !IsArmorCategory(SelectedItemCategoryTag))
	{
		return false;
	}

	if (const FRetrieveArmorDataRow* Row = ArmorDataTable->FindRow<FRetrieveArmorDataRow>(
		SelectedItemId, TEXT("UInventoryPanelWidget::GetSelectedArmorData")))
	{
		OutArmorData = *Row;
		return true;
	}
	return false;
}

bool UInventoryPanelWidget::IsSelectedArmorEquipped() const
{
	return IsArmorCategory(SelectedItemCategoryTag)
		&& IsArmorItemEquipped(SelectedItemId, SelectedSlotInstanceId);
}

bool UInventoryPanelWidget::CanEquipSelectedArmor() const
{
	if (!InventoryComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CanEquipArmor] FAIL: InventoryComponent null"));
		return false;
	}
	if (!ArmorDataTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("[CanEquipArmor] FAIL: ArmorDataTable null"));
		return false;
	}
	if (!IsArmorCategory(SelectedItemCategoryTag))
	{
		UE_LOG(LogTemp, Warning, TEXT("[CanEquipArmor] FAIL: not armor category (tag=%s)"), *SelectedItemCategoryTag.ToString());
		return false;
	}
	if (SelectedItemId.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CanEquipArmor] FAIL: SelectedItemId is None"));
		return false;
	}
	if (!InventoryComponent->CanChangeEquipment())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CanEquipArmor] FAIL: CanChangeEquipment() false"));
		return false;
	}
	if (IsSelectedArmorEquipped())
	{
		UE_LOG(LogTemp, Warning, TEXT("[CanEquipArmor] FAIL: already equipped (ItemId=%s)"), *SelectedItemId.ToString());
		return false;
	}
	UE_LOG(LogTemp, Log, TEXT("[CanEquipArmor] OK: can equip %s"), *SelectedItemId.ToString());
	return true;
}

bool UInventoryPanelWidget::CanUnequipSelectedArmor() const
{
	return InventoryComponent
		&& ArmorDataTable
		&& IsArmorCategory(SelectedItemCategoryTag)
		&& !SelectedItemId.IsNone()
		&& InventoryComponent->CanChangeEquipment()
		&& IsSelectedArmorEquipped();
}

FName UInventoryPanelWidget::GetEquippedArmorIdBySlot(FGameplayTag EquipmentSlotTag) const
{
	return InventoryComponent ? InventoryComponent->GetEquippedArmorId(EquipmentSlotTag) : NAME_None;
}

TArray<FRetrieveEquippedArmorEntry> UInventoryPanelWidget::GetAllEquippedArmorSlots() const
{
	return InventoryComponent ? InventoryComponent->GetEquippedArmorSlots() : TArray<FRetrieveEquippedArmorEntry>();
}

UDataTable* UInventoryPanelWidget::ResolveMaterialItemTable() const
{
	return MaterialItemTable
		? MaterialItemTable.Get()
		: LoadObject<UDataTable>(
			nullptr,
			TEXT("/Game/Retrieve/Data/Items/DT_MaterialItem.DT_MaterialItem"));
}

void UInventoryPanelWidget::RefreshSelectedMaterialDetails()
{
	FRetrieveMaterialItemRow Material;
	if (!GetSelectedMaterialData(Material))
	{
		return;
	}

	const int32 Quantity = InventoryComponent
		? InventoryComponent->GetItemCount(SelectedItemId)
		: 0;
	const FText DisplayName = Material.DisplayName.IsEmpty()
		? FText::FromName(SelectedItemId)
		: Material.DisplayName;

	if (Text_DetailType)
	{
		Text_DetailType->SetText(FText::FromString(TEXT("Material")));
	}
	if (Text_DetailState)
	{
		Text_DetailState->SetText(FText::FromString(FString::Printf(TEXT("Owned: %d"), Quantity)));
	}
	if (Text_DetailName)
	{
		Text_DetailName->SetText(DisplayName);
	}
	if (Text_DetailMainStat)
	{
		Text_DetailMainStat->SetText(FText::FromString(FString::Printf(TEXT("Max Stack: %d"), Material.MaxStack)));
	}
	if (Text_DetailElement)
	{
		Text_DetailElement->SetText(FText::FromString(GetGameplayTagLeaf(Material.ElementTag)));
	}
	if (Text_DetailDescription)
	{
		Text_DetailDescription->SetText(Material.ShortDescription);
	}
}

bool UInventoryPanelWidget::GetItemIconData(FName ItemId, FRetrieveItemIconRow& OutIconData) const
{
	if (!ItemIconTable || ItemId.IsNone())
	{
		return false;
	}

	if (const FRetrieveItemIconRow* Row = ItemIconTable->FindRow<FRetrieveItemIconRow>(ItemId, TEXT("UInventoryPanelWidget::GetItemIconData")))
	{
		OutIconData = *Row;
		return true;
	}
	return false;
}

FText UInventoryPanelWidget::BuildItemTooltipText(FName ItemId, FGameplayTag ItemCategoryTag) const
{
	if (ItemId.IsNone())
	{
		return FText::GetEmpty();
	}

	// 순수 아이템 설명 — Helper에 위임 (DisplayName, 스탯, ShortDescription, SkillPreviews)
	FText BaseDesc = URetrieveItemDescriptionHelper::BuildItemDescription(
		ItemId, ItemCategoryTag,
		ConsumableItemTable, ResolveMaterialItemTable(), WeaponDataTable);

	if (BaseDesc.IsEmpty())
	{
		// 매칭되는 테이블이 없는 경우 폴백
		const int32 Quantity = InventoryComponent ? InventoryComponent->GetItemCount(ItemId) : 0;
		TArray<FString> Lines;
		FRetrieveItemStack FallbackStack;
		FallbackStack.ItemId = ItemId;
		FallbackStack.ItemCategoryTag = ItemCategoryTag;
		FallbackStack.Quantity = Quantity;
		Lines.Add(GetItemDisplayName(FallbackStack));
		if (ItemCategoryTag.IsValid())
		{
			Lines.Add(FString::Printf(TEXT("Type: %s"), *GetItemTypeName(FallbackStack)));
		}
		if (Quantity > 0)
		{
			Lines.Add(FString::Printf(TEXT("Owned: %d"), Quantity));
		}
		return FText::FromString(FString::Join(Lines, TEXT("\n")));
	}

	// 인벤토리 전용 컨텍스트 라인 추가
	const int32 Quantity = InventoryComponent ? InventoryComponent->GetItemCount(ItemId) : 0;
	TArray<FString> ContextLines;

	if (IsWeaponCategory(ItemCategoryTag))
	{
		const bool bSameWeaponTypeEquipped = InventoryComponent
			&& InventoryComponent->GetEquippedWeaponId() == ItemId;
		ContextLines.Add(bSameWeaponTypeEquipped ? TEXT("Equipped") : TEXT("In storage"));
	}
	else if (IsConsumableCategory(ItemCategoryTag) && ConsumableItemTable)
	{
		if (const FRetrieveConsumableItemRow* Row = ConsumableItemTable->FindRow<FRetrieveConsumableItemRow>(
			ItemId, TEXT("UInventoryPanelWidget::BuildItemTooltipText")))
		{
			ContextLines.Add(FString::Printf(TEXT("Owned: %d / Max %d"), Quantity, Row->MaxStack));
		}
		const int32 SlotKey = InventoryComponent
			? InventoryComponent->GetAssignedConsumableSlotKey(ItemId) : INDEX_NONE;
		if (SlotKey != INDEX_NONE)
		{
			ContextLines.Add(FString::Printf(TEXT("Quick Slot: %d"), SlotKey));
		}
	}
	else if (IsMaterialCategory(ItemCategoryTag))
	{
		if (UDataTable* Table = ResolveMaterialItemTable())
		{
			if (const FRetrieveMaterialItemRow* Row = Table->FindRow<FRetrieveMaterialItemRow>(
				ItemId, TEXT("UInventoryPanelWidget::BuildItemTooltipText")))
			{
				ContextLines.Add(FString::Printf(TEXT("Owned: %d / Max %d"), Quantity, Row->MaxStack));
			}
		}
	}
	else if (IsArmorCategory(ItemCategoryTag) && ArmorDataTable)
	{
		if (const FRetrieveArmorDataRow* Row = ArmorDataTable->FindRow<FRetrieveArmorDataRow>(
			ItemId, TEXT("UInventoryPanelWidget::BuildItemTooltipText")))
		{
			ContextLines.Add(FString::Printf(TEXT("방어력: +%.0f"), Row->Defense));
			ContextLines.Add(FString::Printf(TEXT("슬롯: %s"), *GetGameplayTagLeaf(Row->EquipmentSlotTag)));

			const FName EquippedId = InventoryComponent
				? InventoryComponent->GetEquippedArmorId(Row->EquipmentSlotTag) : NAME_None;
			ContextLines.Add(EquippedId == ItemId ? TEXT("장착 중") : TEXT("보관 중"));
		}
	}

	if (ContextLines.IsEmpty())
	{
		return BaseDesc;
	}

	return FText::FromString(BaseDesc.ToString()
		+ TEXT("\n──────────────\n")
		+ FString::Join(ContextLines, TEXT("\n")));
}

void UInventoryPanelWidget::RefreshWeaponComparisonText()
{
	if (Text_SelectedCompare)
	{
		Text_SelectedCompare->SetText(FText::FromString(BuildWeaponComparisonText()));
	}
	if (Text_FinalStatDisplay)
	{
		Text_FinalStatDisplay->SetText(GetFullStatDisplayText());
	}
	RefreshWeaponSkillIcons();
}

void UInventoryPanelWidget::HandleInventoryChanged()
{
	MarkInventoryTooltipsDirty();
	RefreshWeaponComparisonText();
	UpdateQuickSlotPanel();
	UpdateQuickSlotActionButtons();
	OnInventoryListChanged.Broadcast();
	RefreshSelectedMaterialDetails();
}

void UInventoryPanelWidget::HandleCurrencyChanged(int32 NewAmount)
{
	if (Text_CurrencyDisplay)
	{
		Text_CurrencyDisplay->SetText(FText::AsNumber(NewAmount));
	}
}

void UInventoryPanelWidget::HandleEquippedWeaponChanged(FName WeaponItemId)
{
	MarkInventoryTooltipsDirty();
	RefreshWeaponComparisonText();
	RefreshSelectedDetailState();
	UpdateEquipActionButtons();
	OnEquipmentChanged.Broadcast();
	OnInventoryListChanged.Broadcast();
	if (IsWeaponCategory(SelectedItemCategoryTag))
	{
		OnSelectedItemChanged.Broadcast(SelectedItemId, SelectedItemCategoryTag);
	}
	RefreshSlotIcons();
	RefreshStatDisplay();
	RefreshSlotButtonTooltips();
}

FText UInventoryPanelWidget::GetSelectedItemStateText() const
{
	if (SelectedItemId.IsNone() || !InventoryComponent)
	{
		return FText::GetEmpty();
	}

	if (IsWeaponCategory(SelectedItemCategoryTag) || IsConsumableCategory(SelectedItemCategoryTag))
	{
		const int32 SlotKey = InventoryComponent->GetAssignedQuickSlotKey(
			SelectedItemId, SelectedItemCategoryTag);

		if (SlotKey != INDEX_NONE)
		{
			return FText::FromString(FString::Printf(TEXT("퀵슬롯 %d"), SlotKey));
		}

		if (IsWeaponCategory(SelectedItemCategoryTag))
		{
			return IsSelectedWeaponEquipped()
				? INVTEXT("장착 중")
				: INVTEXT("보관 중");
		}

		return INVTEXT("없음");
	}

	if (IsMaterialCategory(SelectedItemCategoryTag))
	{
		const int32 Count = InventoryComponent->GetItemCount(SelectedItemId);
		return FText::Format(INVTEXT("보유 {0}개"), Count);
	}

	if (IsArmorCategory(SelectedItemCategoryTag))
	{
		if (IsSelectedArmorEquipped())
		{
			FRetrieveArmorDataRow ArmorData;
			if (GetSelectedArmorData(ArmorData))
			{
				return FText::Format(INVTEXT("장착 중 ({0})"), ArmorData.DisplayName);
			}
			return INVTEXT("장착 중");
		}
		return INVTEXT("보관 중");
	}

	return FText::GetEmpty();
}

void UInventoryPanelWidget::RefreshSelectedDetailState()
{
	if (Text_DetailState)
	{
		Text_DetailState->SetText(GetSelectedItemStateText());
	}
}

void UInventoryPanelWidget::HandleConfirmEquipClicked()
{
	TGuardValue<bool> BypassGuard(bBypassWeaponSwapConfirm, true);
	EquipSelectedWeapon();
}

void UInventoryPanelWidget::HandleCancelEquipClicked()
{
	ShowWeaponSwapConfirm(false);
}

void UInventoryPanelWidget::HandleAssignQuickSlotClicked()
{
	OpenQuickSlotWheelForAssign();
}

void UInventoryPanelWidget::HandleUnassignQuickSlotClicked()
{
	UnassignSelectedQuickSlot();
}

void UInventoryPanelWidget::HandleAssignSlot4Clicked()
{
	AssignSelectedConsumableToSlot(UInventoryComponent::QuickSlotPrimaryKey);
}

void UInventoryPanelWidget::HandleAssignSlot5Clicked()
{
	AssignSelectedConsumableToSlot(UInventoryComponent::QuickSlotSecondaryKey);
}

void UInventoryPanelWidget::HandleCancelQuickSlotAssignClicked()
{
	HideQuickSlotAssignDialog();
}

void UInventoryPanelWidget::HandleConsumableSlotChanged(int32 SlotKey, FName ItemId)
{
	MarkInventoryTooltipsDirty();
	UpdateQuickSlotPanel();
	UpdateQuickSlotActionButtons();
	OnInventoryListChanged.Broadcast();
}

void UInventoryPanelWidget::HandleMaterialTabClicked()
{
	OpenTab(2);
}

void UInventoryPanelWidget::HandleArmorTabClicked()
{
	OpenTab(3);
}

void UInventoryPanelWidget::HandleEquipClicked()
{
	if (IsWeaponCategory(SelectedItemCategoryTag))
	{
		// 무기 교체 확인 팝업이 필요하면 EquipSelectedWeapon 내부에서 처리
		EquipSelectedWeapon();
	}
	else if (IsArmorCategory(SelectedItemCategoryTag))
	{
		EquipSelectedArmor();
	}
}

void UInventoryPanelWidget::HandleUnequipClicked()
{
	if (IsWeaponCategory(SelectedItemCategoryTag))
	{
		UnequipCurrentWeapon();
	}
	else if (IsArmorCategory(SelectedItemCategoryTag))
	{
		UnequipSelectedArmor();
	}
}

void UInventoryPanelWidget::HandleEquippedArmorChanged(FGameplayTag EquipmentSlotTag, FName ArmorItemId)
{
	MarkInventoryTooltipsDirty();
	RefreshWeaponComparisonText();
	RefreshSelectedDetailState();
	UpdateEquipActionButtons();
	// WBP는 이 공용 이벤트에서 스탯/장비 패널을 다시 그린다.
	// 무기 변경 경로와 달리 방어구 경로에는 이 Broadcast가 빠져 있어
	// 실제 Defense가 반영돼도 화면의 스탯 값은 이전 값으로 남아 있었다.
	OnEquipmentChanged.Broadcast();
	OnInventoryListChanged.Broadcast();
	if (IsArmorCategory(SelectedItemCategoryTag))
	{
		RefreshSelectedArmorDetails();
	}
	RefreshSlotIcons();
	RefreshStatDisplay();
	RefreshSlotButtonTooltips();
}

void UInventoryPanelWidget::BindInventoryEvents()
{
	if (InventoryComponent)
	{
		// 위젯 재생성/초기화 반복 시 델리게이트 중복 등록 방지
		InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &ThisClass::HandleInventoryChanged);
		InventoryComponent->OnInventoryChanged.AddDynamic(this, &ThisClass::HandleInventoryChanged);

		InventoryComponent->OnEquippedWeaponChanged.RemoveDynamic(this, &ThisClass::HandleEquippedWeaponChanged);
		InventoryComponent->OnEquippedWeaponChanged.AddDynamic(this, &ThisClass::HandleEquippedWeaponChanged);

		InventoryComponent->OnConsumableSlotChanged.RemoveDynamic(this, &ThisClass::HandleConsumableSlotChanged);
		InventoryComponent->OnConsumableSlotChanged.AddDynamic(this, &ThisClass::HandleConsumableSlotChanged);

		InventoryComponent->OnEquippedArmorChanged.RemoveDynamic(this, &ThisClass::HandleEquippedArmorChanged);
		InventoryComponent->OnEquippedArmorChanged.AddDynamic(this, &ThisClass::HandleEquippedArmorChanged);

		InventoryComponent->OnCurrencyChanged.RemoveDynamic(this, &ThisClass::HandleCurrencyChanged);
		InventoryComponent->OnCurrencyChanged.AddDynamic(this, &ThisClass::HandleCurrencyChanged);
		if (Text_CurrencyDisplay)
		{
			Text_CurrencyDisplay->SetText(FText::AsNumber(InventoryComponent->GetCurrency()));
		}
	}
}

void UInventoryPanelWidget::BindButtonEvents()
{
	if (Button_TabMaterial)
	{
		Button_TabMaterial->OnClicked.RemoveDynamic(this, &ThisClass::HandleMaterialTabClicked);
		Button_TabMaterial->OnClicked.AddDynamic(this, &ThisClass::HandleMaterialTabClicked);
	}
	if (Button_TabArmor)
	{
		Button_TabArmor->OnClicked.RemoveDynamic(this, &ThisClass::HandleArmorTabClicked);
		Button_TabArmor->OnClicked.AddDynamic(this, &ThisClass::HandleArmorTabClicked);
	}
	if (Button_Equip)
	{
		Button_Equip->OnClicked.RemoveDynamic(this, &ThisClass::HandleEquipClicked);
		Button_Equip->OnClicked.AddDynamic(this, &ThisClass::HandleEquipClicked);
	}
	if (Button_Unequip)
	{
		Button_Unequip->OnClicked.RemoveDynamic(this, &ThisClass::HandleUnequipClicked);
		Button_Unequip->OnClicked.AddDynamic(this, &ThisClass::HandleUnequipClicked);
	}

	// BindWidgetOptional이므로 BP에서 위젯을 배치하지 않았을 수 있음
	if (Button_ConfirmEquip)
	{
		Button_ConfirmEquip->OnClicked.RemoveDynamic(this, &ThisClass::HandleConfirmEquipClicked);
		Button_ConfirmEquip->OnClicked.AddDynamic(this, &ThisClass::HandleConfirmEquipClicked);
	}
	if (Button_CancelEquip)
	{
		Button_CancelEquip->OnClicked.RemoveDynamic(this, &ThisClass::HandleCancelEquipClicked);
		Button_CancelEquip->OnClicked.AddDynamic(this, &ThisClass::HandleCancelEquipClicked);
	}
	if (Button_AssignQuickSlot)
	{
		Button_AssignQuickSlot->OnClicked.RemoveDynamic(this, &ThisClass::HandleAssignQuickSlotClicked);
		Button_AssignQuickSlot->OnClicked.AddDynamic(this, &ThisClass::HandleAssignQuickSlotClicked);
	}
	if (Button_UnassignQuickSlot)
	{
		Button_UnassignQuickSlot->OnClicked.RemoveDynamic(this, &ThisClass::HandleUnassignQuickSlotClicked);
		Button_UnassignQuickSlot->OnClicked.AddDynamic(this, &ThisClass::HandleUnassignQuickSlotClicked);
	}
	if (Button_AssignSlot4)
	{
		Button_AssignSlot4->OnClicked.RemoveDynamic(this, &ThisClass::HandleAssignSlot4Clicked);
		Button_AssignSlot4->OnClicked.AddDynamic(this, &ThisClass::HandleAssignSlot4Clicked);
	}
	if (Button_AssignSlot5)
	{
		Button_AssignSlot5->OnClicked.RemoveDynamic(this, &ThisClass::HandleAssignSlot5Clicked);
		Button_AssignSlot5->OnClicked.AddDynamic(this, &ThisClass::HandleAssignSlot5Clicked);
	}
	if (Button_CancelQuickSlotAssign)
	{
		Button_CancelQuickSlotAssign->OnClicked.RemoveDynamic(this, &ThisClass::HandleCancelQuickSlotAssignClicked);
		Button_CancelQuickSlotAssign->OnClicked.AddDynamic(this, &ThisClass::HandleCancelQuickSlotAssignClicked);
	}

	if (Button_ConfirmQuickSlotReplace)
	{
		Button_ConfirmQuickSlotReplace->OnClicked.RemoveDynamic(this, &ThisClass::ConfirmQuickSlotReplace);
		Button_ConfirmQuickSlotReplace->OnClicked.AddDynamic(this, &ThisClass::ConfirmQuickSlotReplace);
	}

	if (Button_CancelQuickSlotReplace)
	{
		Button_CancelQuickSlotReplace->OnClicked.RemoveDynamic(this, &ThisClass::CancelQuickSlotReplace);
		Button_CancelQuickSlotReplace->OnClicked.AddDynamic(this, &ThisClass::CancelQuickSlotReplace);
	}

	// 장착 프리뷰 슬롯 버튼
	if (Button_SlotHead)
	{
		Button_SlotHead->OnClicked.RemoveDynamic(this, &ThisClass::HandleSlotHeadClicked);
		Button_SlotHead->OnClicked.AddDynamic(this, &ThisClass::HandleSlotHeadClicked);
	}
	if (Button_SlotWeapon)
	{
		Button_SlotWeapon->OnClicked.RemoveDynamic(this, &ThisClass::HandleSlotWeaponClicked);
		Button_SlotWeapon->OnClicked.AddDynamic(this, &ThisClass::HandleSlotWeaponClicked);
	}
	if (Button_SlotChest)
	{
		Button_SlotChest->OnClicked.RemoveDynamic(this, &ThisClass::HandleSlotChestClicked);
		Button_SlotChest->OnClicked.AddDynamic(this, &ThisClass::HandleSlotChestClicked);
	}
	if (Button_SlotHands_L)
	{
		Button_SlotHands_L->OnClicked.RemoveDynamic(this, &ThisClass::HandleSlotHandsLClicked);
		Button_SlotHands_L->OnClicked.AddDynamic(this, &ThisClass::HandleSlotHandsLClicked);
	}
	if (Button_SlotHands_R)
	{
		Button_SlotHands_R->OnClicked.RemoveDynamic(this, &ThisClass::HandleSlotHandsRClicked);
		Button_SlotHands_R->OnClicked.AddDynamic(this, &ThisClass::HandleSlotHandsRClicked);
	}
	if (Button_SlotLegs)
	{
		Button_SlotLegs->OnClicked.RemoveDynamic(this, &ThisClass::HandleSlotLegsClicked);
		Button_SlotLegs->OnClicked.AddDynamic(this, &ThisClass::HandleSlotLegsClicked);
	}
	if (Button_SlotFeet)
	{
		Button_SlotFeet->OnClicked.RemoveDynamic(this, &ThisClass::HandleSlotFeetClicked);
		Button_SlotFeet->OnClicked.AddDynamic(this, &ThisClass::HandleSlotFeetClicked);
	}

	RegisterSoundButton(Button_TabMaterial);
	RegisterSoundButton(Button_TabArmor);
	RegisterSoundButton(Button_Equip);
	RegisterSoundButton(Button_Unequip);
	RegisterSoundButton(Button_ConfirmEquip);
	RegisterSoundButton(Button_CancelEquip);
	RegisterSoundButton(Button_AssignQuickSlot);
	RegisterSoundButton(Button_UnassignQuickSlot);
	RegisterSoundButton(Button_AssignSlot4);
	RegisterSoundButton(Button_AssignSlot5);
	RegisterSoundButton(Button_CancelQuickSlotAssign);
	RegisterSoundButton(Button_SortAttackPower);
	RegisterSoundButton(Button_SortDefense);
	RegisterSoundButton(Button_SlotHead);
	RegisterSoundButton(Button_SlotWeapon);
	RegisterSoundButton(Button_SlotChest);
	RegisterSoundButton(Button_SlotHands_L);
	RegisterSoundButton(Button_SlotHands_R);
	RegisterSoundButton(Button_SlotLegs);
	RegisterSoundButton(Button_SlotFeet);

	RefreshSlotButtonTooltips();
}


void UInventoryPanelWidget::InitOwnerComponents()
{
	if (InventoryComponent && WeaponComponent)
	{
		return;
	}

	APawn* OwningPawn = GetOwningPlayerPawn();
	if (!OwningPawn)
	{
		return;
	}

	if (!InventoryComponent)
	{
		InventoryComponent = OwningPawn->FindComponentByClass<UInventoryComponent>();
	}
	if (!WeaponComponent)
	{
		WeaponComponent = OwningPawn->FindComponentByClass<UWeaponComponent>();
	}
}

void UInventoryPanelWidget::InitDefaultTags()
{
	if (!WeaponTabCategoryTag.IsValid())
	{
		WeaponTabCategoryTag = RetrieveGameplayTags::Item_Weapon;
	}
	if (!ConsumableTabCategoryTag.IsValid())
	{
		ConsumableTabCategoryTag = RetrieveGameplayTags::Item_Consumable;
	}
	if (!MaterialTabCategoryTag.IsValid())
	{
		MaterialTabCategoryTag = RetrieveGameplayTags::Item_Material;
	}
	if (!ArmorTabCategoryTag.IsValid())
	{
		ArmorTabCategoryTag = RetrieveGameplayTags::Item_Armor;
	}
	if (!CurrentCategoryTag.IsValid())
	{
		if (ActiveTabIndex == 1)      { CurrentCategoryTag = ConsumableTabCategoryTag; }
		else if (ActiveTabIndex == 2) { CurrentCategoryTag = MaterialTabCategoryTag; }
		else if (ActiveTabIndex == 3) { CurrentCategoryTag = ArmorTabCategoryTag; }
		else                          { CurrentCategoryTag = WeaponTabCategoryTag; }
	}
}

void UInventoryPanelWidget::ResolveDefaultTooltipWidgetClasses()
{
	if (!ItemDetailTooltipWidgetClass)
	{
		ItemDetailTooltipWidgetClass = LoadClass<UUserWidget>(
			nullptr,
			TEXT("/Game/Retrieve/UI/Inventory/WBP_ItemDetailTooltip.WBP_ItemDetailTooltip_C"));
	}

	if (!ItemCompareTooltipWidgetClass)
	{
		ItemCompareTooltipWidgetClass = LoadClass<UUserWidget>(
			nullptr,
			TEXT("/Game/Retrieve/UI/Inventory/WBP_ItemCompareTooltip.WBP_ItemCompareTooltip_C"));
	}
}

void UInventoryPanelWidget::RefreshInventoryView(bool bClearSelection)
{
	InitDefaultTags();
	MarkInventoryTooltipsDirty();

	if (bClearSelection)
	{
		ClearSelection();
	}

	OnInventoryListChanged.Broadcast();
}

void UInventoryPanelWidget::ClearSelection()
{
	SelectedItemId = NAME_None;
	SelectedItemCategoryTag = FGameplayTag();
	SelectedSlotInstanceId = INDEX_NONE;
	ShowWeaponSwapConfirm(false);
	HideQuickSlotAssignDialog();
	ShowQuickSlotReplaceConfirm(false);
	RefreshWeaponComparisonText();
	UpdateQuickSlotActionButtons();
	UpdateEquipActionButtons();
	OnSelectedItemChanged.Broadcast(SelectedItemId, SelectedItemCategoryTag);
}

bool UInventoryPanelWidget::IsWeaponCategory(FGameplayTag ItemCategoryTag) const
{
	return ItemCategoryTag.IsValid() && ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Weapon);
}

bool UInventoryPanelWidget::IsConsumableCategory(FGameplayTag ItemCategoryTag) const
{
	return ItemCategoryTag.IsValid() && ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Consumable);
}

bool UInventoryPanelWidget::IsMaterialCategory(FGameplayTag ItemCategoryTag) const
{
	return ItemCategoryTag.IsValid() && ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Material);
}

bool UInventoryPanelWidget::IsArmorCategory(FGameplayTag ItemCategoryTag) const
{
	return ItemCategoryTag.IsValid() && ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Armor);
}

bool UInventoryPanelWidget::CanEquipSelected() const
{
	if (IsWeaponCategory(SelectedItemCategoryTag)) return CanEquipSelectedWeapon();
	if (IsArmorCategory(SelectedItemCategoryTag))  return CanEquipSelectedArmor();
	return false;
}

bool UInventoryPanelWidget::CanUnequipSelected() const
{
	if (IsWeaponCategory(SelectedItemCategoryTag)) return CanUnequipCurrentWeapon();
	if (IsArmorCategory(SelectedItemCategoryTag))  return CanUnequipSelectedArmor();
	return false;
}

void UInventoryPanelWidget::SelectEquipmentSlot(FGameplayTag EquipmentSlotTag)
{
	if (!InventoryComponent || !EquipmentSlotTag.IsValid())
	{
		return;
	}

	// 무기 슬롯 (Equipment.Slot 태그가 없고 WeaponTag와 구분) — 무기는 별도 처리
	// 방어구 슬롯: 해당 슬롯에 장착된 아이템을 목록에서 선택
	const FName EquippedArmorId = InventoryComponent->GetEquippedArmorId(EquipmentSlotTag);
	if (!EquippedArmorId.IsNone())
	{
		// 방어구 탭으로 전환 후 해당 아이템 선택
		if (!IsArmorCategory(CurrentCategoryTag))
		{
			OpenTab(3);
		}
		int32 EquippedSlotInstanceId = INDEX_NONE;
		for (const FRetrieveEquippedArmorEntry& EquippedSlot : InventoryComponent->GetEquippedArmorSlots())
		{
			if (EquippedSlot.EquipmentSlotTag == EquipmentSlotTag)
			{
				EquippedSlotInstanceId = EquippedSlot.SlotInstanceId;
				break;
			}
		}
		SelectItem(EquippedArmorId, ArmorTabCategoryTag, EquippedSlotInstanceId);
		return;
	}

	// 슬롯이 비어있으면 탭만 전환
	if (!IsArmorCategory(CurrentCategoryTag))
	{
		OpenTab(3);
	}
}

void UInventoryPanelWidget::RefreshSelectedArmorDetails()
{
	FRetrieveArmorDataRow Armor;
	if (!GetSelectedArmorData(Armor))
	{
		return;
	}

	if (Text_DetailType)
	{
		Text_DetailType->SetText(FText::FromString(GetGameplayTagLeaf(Armor.EquipmentSlotTag)));
	}
	if (Text_DetailState)
	{
		Text_DetailState->SetText(GetSelectedItemStateText());
	}
	if (Text_DetailName)
	{
		Text_DetailName->SetText(Armor.DisplayName);
	}
	if (Text_DetailMainStat)
	{
		const float TotalDef = GetTotalDefense();
		const FString StatText = IsSelectedArmorEquipped()
			? FString::Printf(TEXT("방어력: +%.0f  (최종: %.0f)"), Armor.Defense, TotalDef)
			: FString::Printf(TEXT("방어력: +%.0f"), Armor.Defense);
		Text_DetailMainStat->SetText(FText::FromString(StatText));
	}
	if (Text_DetailDescription)
	{
		Text_DetailDescription->SetText(Armor.ShortDescription);
	}
}

void UInventoryPanelWidget::UpdateEquipActionButtons()
{
	// 표시 여부: 장비 변경 잠금과 무관하게 '대상이 있는지'로만 결정한다.
	// (장착 시 발검으로 잠시 전투상태가 되어 CanChangeEquipment가 false가 되더라도 버튼은 보여야 한다)
	const bool bShowEquip   = ShouldShowEquipButton();
	const bool bShowUnequip = ShouldShowUnequipButton();
	// 활성화(클릭 가능) 여부: 실제로 지금 변경 가능한지 (CanChangeEquipment 잠금 포함)
	const bool bCanEquip    = CanEquipSelected();
	const bool bCanUnequip  = CanUnequipSelected();

	if (Button_Equip)
	{
		Button_Equip->SetVisibility(bShowEquip ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Button_Equip->SetIsEnabled(bCanEquip);
	}
	if (Button_Unequip)
	{
		Button_Unequip->SetVisibility(bShowUnequip ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		Button_Unequip->SetIsEnabled(bCanUnequip);
	}
}

bool UInventoryPanelWidget::ShouldShowEquipButton() const
{
	if (!InventoryComponent || SelectedItemId.IsNone())
	{
		return false;
	}
	if (IsWeaponCategory(SelectedItemCategoryTag))
	{
		return !IsSelectedWeaponEquipped();
	}
	if (IsArmorCategory(SelectedItemCategoryTag))
	{
		return ArmorDataTable && !IsSelectedArmorEquipped();
	}
	return false;
}

bool UInventoryPanelWidget::ShouldShowUnequipButton() const
{
	if (!InventoryComponent)
	{
		return false;
	}
	if (IsWeaponCategory(SelectedItemCategoryTag))
	{
		// 무기 해제는 현재 장착 무기를 대상으로 하므로, 무기 탭에서 장착 무기가 있으면 표시
		return !InventoryComponent->GetEquippedWeaponId().IsNone();
	}
	if (IsArmorCategory(SelectedItemCategoryTag))
	{
		return ArmorDataTable && !SelectedItemId.IsNone() && IsSelectedArmorEquipped();
	}
	return false;
}

bool UInventoryPanelWidget::ShouldConfirmWeaponSwap() const
{
	if (!InventoryComponent || SelectedItemId.IsNone() || !IsWeaponCategory(SelectedItemCategoryTag))
	{
		return false;
	}

	const FName EquippedWeaponId = InventoryComponent->GetEquippedWeaponId();
	return !EquippedWeaponId.IsNone() && EquippedWeaponId != SelectedItemId;
}

void UInventoryPanelWidget::OpenQuickSlotWheelForAssign()
{
	if (!CanAssignSelectedItemToQuickSlot())
	{
		return;
	}

	if (!QuickSlotWheelInstance && QuickSlotWheelClass)
	{
		QuickSlotWheelInstance = CreateWidget<URetrieveQuickSlotWheelWidget>(
			GetOwningPlayer(),
			QuickSlotWheelClass);

		if (QuickSlotWheelInstance)
		{
			QuickSlotWheelInstance->AddToViewport(80);
			QuickSlotWheelInstance->InitializeQuickSlotWheel(InventoryComponent, ItemIconTable);

			QuickSlotWheelInstance->OnWheelSlotClicked.RemoveDynamic(
				this, &ThisClass::HandleQuickSlotWheelSlotClicked);
			QuickSlotWheelInstance->OnWheelSlotClicked.AddDynamic(
				this, &ThisClass::HandleQuickSlotWheelSlotClicked);
		}
	}

	if (QuickSlotWheelInstance)
	{
		QuickSlotWheelInstance->OpenForAssign(SelectedItemId, SelectedItemCategoryTag);
	}
}

void UInventoryPanelWidget::HandleQuickSlotWheelSlotClicked(int32 SlotKey)
{
	if (!InventoryComponent || SelectedItemId.IsNone())
	{
		return;
	}

	const FRetrieveQuickSlotEntry CurrentEntry = InventoryComponent->GetQuickSlotEntry(SlotKey);

	if (!CurrentEntry.IsValid())
	{
		// 빈 슬롯 → 즉시 등록
		AssignSelectedItemToQuickSlot(SlotKey, true);

		if (QuickSlotWheelInstance)
		{
			QuickSlotWheelInstance->SetVisibility(ESlateVisibility::Collapsed);
		}
		return;
	}

	// 이미 아이템이 있으면 교체 확인
	PendingReplaceSlotKey = SlotKey;
	PendingQuickSlotItemId = SelectedItemId;
	PendingQuickSlotCategoryTag = SelectedItemCategoryTag;

	// 휠이 뷰포트 위(z80)를 덮은 채로 두면 교체 다이얼로그 버튼 클릭을 가로채므로 먼저 닫는다
	if (QuickSlotWheelInstance)
	{
		QuickSlotWheelInstance->SetVisibility(ESlateVisibility::Collapsed);
	}

	ShowQuickSlotReplaceConfirm(true);
}

bool UInventoryPanelWidget::AssignSelectedItemToQuickSlot(int32 SlotKey, bool bForceReplace)
{
	if (!InventoryComponent || !CanAssignSelectedItemToQuickSlot())
	{
		return false;
	}

	const FRetrieveQuickSlotEntry CurrentEntry = InventoryComponent->GetQuickSlotEntry(SlotKey);

	if (CurrentEntry.IsValid() && !bForceReplace)
	{
		PendingReplaceSlotKey = SlotKey;
		PendingQuickSlotItemId = SelectedItemId;
		PendingQuickSlotCategoryTag = SelectedItemCategoryTag;
		ShowQuickSlotReplaceConfirm(true);
		return false;
	}

	const bool bAssigned = InventoryComponent->AssignQuickSlotItem(
		SlotKey, SelectedItemId, SelectedItemCategoryTag);

	if (bAssigned)
	{
		MarkInventoryTooltipsDirty();
		HideQuickSlotAssignDialog();
		ShowQuickSlotReplaceConfirm(false);
		UpdateQuickSlotPanel();
		UpdateQuickSlotActionButtons();
		OnInventoryListChanged.Broadcast();

		if (QuickSlotWheelInstance)
		{
			QuickSlotWheelInstance->RefreshFromQuickSlots();
		}
	}

	return bAssigned;
}

bool UInventoryPanelWidget::UnassignSelectedQuickSlot()
{
	if (!InventoryComponent || !IsSelectedItemAssignedToQuickSlot())
	{
		return false;
	}

	const int32 SlotKey = GetSelectedQuickSlotKey();
	const bool bUnassigned = InventoryComponent->UnassignQuickSlotItem(SlotKey);

	if (bUnassigned)
	{
		MarkInventoryTooltipsDirty();
		HideQuickSlotAssignDialog();
		ShowQuickSlotReplaceConfirm(false);
		UpdateQuickSlotPanel();
		UpdateQuickSlotActionButtons();
		OnInventoryListChanged.Broadcast();

		if (QuickSlotWheelInstance)
		{
			QuickSlotWheelInstance->RefreshFromQuickSlots();
		}
	}

	return bUnassigned;
}

void UInventoryPanelWidget::ConfirmQuickSlotReplace()
{
	if (!InventoryComponent || PendingReplaceSlotKey == INDEX_NONE)
	{
		ShowQuickSlotReplaceConfirm(false);
		return;
	}

	const bool bAssigned = InventoryComponent->AssignQuickSlotItem(
		PendingReplaceSlotKey,
		PendingQuickSlotItemId,
		PendingQuickSlotCategoryTag);

	if (bAssigned)
	{
		MarkInventoryTooltipsDirty();
		UpdateQuickSlotPanel();
		UpdateQuickSlotActionButtons();
		OnInventoryListChanged.Broadcast();

		if (QuickSlotWheelInstance)
		{
			QuickSlotWheelInstance->RefreshFromQuickSlots();
			QuickSlotWheelInstance->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	PendingReplaceSlotKey = INDEX_NONE;
	PendingQuickSlotItemId = NAME_None;
	PendingQuickSlotCategoryTag = FGameplayTag();

	ShowQuickSlotReplaceConfirm(false);
}

void UInventoryPanelWidget::CancelQuickSlotReplace()
{
	PendingReplaceSlotKey = INDEX_NONE;
	PendingQuickSlotItemId = NAME_None;
	PendingQuickSlotCategoryTag = FGameplayTag();

	ShowQuickSlotReplaceConfirm(false);
}

void UInventoryPanelWidget::ShowQuickSlotReplaceConfirm(bool bShow)
{
	if (Border_QuickSlotReplaceConfirm)
	{
		Border_QuickSlotReplaceConfirm->SetVisibility(
			bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (Text_QuickSlotReplaceMessage && bShow)
	{
		Text_QuickSlotReplaceMessage->SetText(
			INVTEXT("이미 아이템이 등록된 슬롯입니다.\n교체하시겠습니까?"));
	}
}

void UInventoryPanelWidget::ShowWeaponSwapConfirm(bool bShow)
{
	if (Border_WeaponSwapConfirm)
	{
		Border_WeaponSwapConfirm->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UInventoryPanelWidget::UpdateQuickSlotPanel()
{
	if (Text_QuickSlot4)
	{
		Text_QuickSlot4->SetText(GetQuickSlotDisplayText(UInventoryComponent::QuickSlotPrimaryKey));
	}
	if (Text_QuickSlot5)
	{
		Text_QuickSlot5->SetText(GetQuickSlotDisplayText(UInventoryComponent::QuickSlotSecondaryKey));
	}
}

void UInventoryPanelWidget::UpdateQuickSlotActionButtons()
{
	const bool bCanAssign = CanAssignSelectedItemToQuickSlot();
	const bool bIsAssigned = IsSelectedItemAssignedToQuickSlot();

	if (Button_AssignQuickSlot)
	{
		Button_AssignQuickSlot->SetVisibility(
			bCanAssign && !bIsAssigned
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
		Button_AssignQuickSlot->SetIsEnabled(bCanAssign && !bIsAssigned);
	}

	if (Button_UnassignQuickSlot)
	{
		Button_UnassignQuickSlot->SetVisibility(
			bCanAssign && bIsAssigned
				? ESlateVisibility::Visible
				: ESlateVisibility::Collapsed);
		Button_UnassignQuickSlot->SetIsEnabled(bCanAssign && bIsAssigned);
	}

	if (!bCanAssign)
	{
		HideQuickSlotAssignDialog();
		ShowQuickSlotReplaceConfirm(false);
	}
}

void UInventoryPanelWidget::RefreshInventoryGridLayout()
{
	if (!UniformGrid_ItemList)
	{
		return;
	}

	constexpr int32 GridColumnCount = 6;
	constexpr float ScrollbarAllowance = 14.0f;

	const FVector2D GridAreaSize = ScrollBox_ItemList
		? ScrollBox_ItemList->GetCachedGeometry().GetLocalSize()
		: UniformGrid_ItemList->GetCachedGeometry().GetLocalSize();
	if (GridAreaSize.X <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float AvailableWidth = FMath::Max(GridAreaSize.X - ScrollbarAllowance, 1.0f);
	const float SlotSize = FMath::Max(FMath::FloorToFloat(AvailableWidth / static_cast<float>(GridColumnCount)), 1.0f);
	const bool bGridAreaChanged = !GridAreaSize.Equals(LastInventoryGridAreaSize, 0.5f);

	LastInventoryGridAreaSize = GridAreaSize;

	const int32 ChildCount = UniformGrid_ItemList->GetChildrenCount();

	// 슬롯 정사각 크기 + Fill 정렬을 매 갱신마다 적용한다.
	// 영역 변화 시 한 번만 적용하면, 슬롯이 그 뒤에 채워지거나 콘텐츠 크기가
	// 늦게 확정될 때 셀이 늘어난 채로 굳는다(탭 전환으로 슬롯을 재생성해야만
	// 정상화되던 원인). 매 틱 강제하면 콘텐츠 확정 즉시 정사각으로 맞춰진다.
	UniformGrid_ItemList->SetMinDesiredSlotWidth(SlotSize);
	UniformGrid_ItemList->SetMinDesiredSlotHeight(SlotSize);
	for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
	{
		UWidget* Child = UniformGrid_ItemList->GetChildAt(ChildIndex);
		if (UUniformGridSlot* GridSlot = Cast<UUniformGridSlot>(Child ? Child->Slot : nullptr))
		{
			GridSlot->SetHorizontalAlignment(HAlign_Fill);
			GridSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}
	const FName EquippedWeaponId = InventoryComponent ? InventoryComponent->GetEquippedWeaponId() : NAME_None;
	const bool bTooltipCacheSizeChanged =
		AppliedTooltipItemIds.Num() != ChildCount
		|| AppliedTooltipCategoryTags.Num() != ChildCount
		|| AppliedTooltipCompareFlags.Num() != ChildCount
		|| AppliedTooltipCompareReferenceIds.Num() != ChildCount;
	const bool bEquippedWeaponChanged = AppliedTooltipEquippedWeaponId != EquippedWeaponId;

	if (bGridAreaChanged || bInventoryTooltipsDirty || bTooltipCacheSizeChanged || bEquippedWeaponChanged)
	{
		ApplyInventorySlotTooltips();
	}
}

void UInventoryPanelWidget::MarkInventoryTooltipsDirty()
{
	bInventoryTooltipsDirty = true;
}

void UInventoryPanelWidget::ApplyInventorySlotTooltips()
{
	if (!UniformGrid_ItemList)
	{
		return;
	}

	const int32 ChildCount = UniformGrid_ItemList->GetChildrenCount();
	const TArray<FRetrieveItemStack> Items = GetCurrentItemsSorted();
	AppliedTooltipItemIds.SetNum(ChildCount);
	AppliedTooltipCategoryTags.SetNum(ChildCount);
	AppliedTooltipCompareFlags.SetNum(ChildCount);
	AppliedTooltipCompareReferenceIds.SetNum(ChildCount);

	bool bAnyUpdateDeferredDueToHover = false;

	for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
	{
		UWidget* Child = UniformGrid_ItemList->GetChildAt(ChildIndex);

		if (!Child)
		{
			continue;
		}

		// HoverDelay 무력화 등 레거시 BP 툴팁 억제는 네이티브 SetToolTip과 무관하므로 매 패스 안전하게 반복 적용한다.
		DisableLegacyTooltipRecursive(Child);

		if (!Items.IsValidIndex(ChildIndex))
		{
			if (AppliedTooltipItemIds[ChildIndex] != NAME_None || Child->GetToolTip() != nullptr)
			{
				ClearWidgetTooltipRecursive(Child);
				Child->SetToolTip(nullptr);
			}
			AppliedTooltipItemIds[ChildIndex] = NAME_None;
			AppliedTooltipCategoryTags[ChildIndex] = FGameplayTag();
			AppliedTooltipCompareFlags[ChildIndex] = false;
			AppliedTooltipCompareReferenceIds[ChildIndex] = NAME_None;
			continue;
		}

		const FRetrieveItemStack& Item = Items[ChildIndex];
		const bool bUseCompareTooltip = ShouldUseCompareTooltipForItem(Item);
		// 비교 대상(현재 장착된 무기/방어구)이 바뀌면 아이템/카테고리/비교여부가 그대로여도
		// 툴팁 내용(Current 스탯)이 갱신돼야 한다.
		const FName CompareReferenceId = bUseCompareTooltip ? GetCompareReferenceItemId(Item) : NAME_None;
		const bool bTooltipAlreadyApplied =
			AppliedTooltipItemIds[ChildIndex] == Item.ItemId
			&& AppliedTooltipCategoryTags[ChildIndex] == Item.ItemCategoryTag
			&& AppliedTooltipCompareFlags[ChildIndex] == bUseCompareTooltip
			&& AppliedTooltipCompareReferenceIds[ChildIndex] == CompareReferenceId
			&& Child->GetToolTip() != nullptr;

		if (bTooltipAlreadyApplied)
		{
			continue;
		}

		// 지금 마우스가 올라가 있는 슬롯의 툴팁을 표시 도중 교체하면 Slate 팝업이
		// 엉뚱한 위치에 뜨거나, 안 사라지거나, 신/구 팝업이 동시에 보이는 문제가 생긴다.
		// 호버가 끝날 때까지 이 슬롯의 갱신을 미루고 dirty 상태를 유지해 다음 패스에서 재시도한다.
		if (IsWidgetOrDescendantHovered(Child))
		{
			bAnyUpdateDeferredDueToHover = true;
			continue;
		}

		ClearWidgetTooltipRecursive(Child);
		Child->SetToolTip(CreateInventorySlotTooltip(Item));
		AppliedTooltipItemIds[ChildIndex] = Item.ItemId;
		AppliedTooltipCategoryTags[ChildIndex] = Item.ItemCategoryTag;
		AppliedTooltipCompareFlags[ChildIndex] = bUseCompareTooltip;
		AppliedTooltipCompareReferenceIds[ChildIndex] = CompareReferenceId;
	}

	AppliedTooltipEquippedWeaponId = InventoryComponent ? InventoryComponent->GetEquippedWeaponId() : NAME_None;
	bInventoryTooltipsDirty = bAnyUpdateDeferredDueToHover;
}

void UInventoryPanelWidget::ClearWidgetTooltipRecursive(UWidget* Widget) const
{
	if (!Widget)
	{
		return;
	}

	Widget->SetToolTipText(FText::GetEmpty());
	Widget->SetToolTip(nullptr);

	if (UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
	{
		if (UserWidget->WidgetTree)
		{
			UserWidget->WidgetTree->ForEachWidget([this, Widget](UWidget* TreeWidget)
			{
				if (TreeWidget && TreeWidget != Widget)
				{
					ClearWidgetTooltipRecursive(TreeWidget);
				}
			});
		}
	}

	if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget))
	{
		const int32 ChildCount = PanelWidget->GetChildrenCount();
		for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
		{
			ClearWidgetTooltipRecursive(PanelWidget->GetChildAt(ChildIndex));
		}
	}
}

void UInventoryPanelWidget::DisableLegacyTooltipRecursive(UWidget* Widget) const
{
	if (!Widget)
	{
		return;
	}

	if (UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
	{
		if (FFloatProperty* HoverDelayProperty = FindFProperty<FFloatProperty>(UserWidget->GetClass(), TEXT("HoverDelay")))
		{
			HoverDelayProperty->SetPropertyValue_InContainer(UserWidget, TNumericLimits<float>::Max());
		}

		if (UFunction* HideFunction = UserWidget->FindFunction(TEXT("HideTooltip")))
		{
			UserWidget->ProcessEvent(HideFunction, nullptr);
		}

		if (UserWidget->WidgetTree)
		{
			UserWidget->WidgetTree->ForEachWidget([this, Widget](UWidget* TreeWidget)
			{
				if (TreeWidget && TreeWidget != Widget)
				{
					DisableLegacyTooltipRecursive(TreeWidget);
				}
			});
		}
	}

	if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget))
	{
		const int32 ChildCount = PanelWidget->GetChildrenCount();
		for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
		{
			DisableLegacyTooltipRecursive(PanelWidget->GetChildAt(ChildIndex));
		}
	}
}

void UInventoryPanelWidget::SuppressBlueprintManagedTooltip()
{
	if (UFunction* HideFunction = FindFunction(TEXT("HideTooltip")))
	{
		ProcessEvent(HideFunction, nullptr);
	}

	if (FObjectProperty* TooltipRefProperty = FindFProperty<FObjectProperty>(GetClass(), TEXT("CurrentTooltipRef")))
	{
		if (UWidget* CurrentTooltip = Cast<UWidget>(TooltipRefProperty->GetObjectPropertyValue_InContainer(this)))
		{
			CurrentTooltip->SetVisibility(ESlateVisibility::Collapsed);
			CurrentTooltip->RemoveFromParent();
			TooltipRefProperty->SetObjectPropertyValue_InContainer(this, nullptr);
		}
	}
}

bool UInventoryPanelWidget::IsWidgetOrDescendantHovered(const UWidget* Widget) const
{
	if (!Widget)
	{
		return false;
	}

	if (Widget->IsHovered())
	{
		return true;
	}

	if (const UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget))
	{
		const int32 ChildCount = PanelWidget->GetChildrenCount();
		for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
		{
			if (IsWidgetOrDescendantHovered(PanelWidget->GetChildAt(ChildIndex)))
			{
				return true;
			}
		}
	}

	if (const UUserWidget* UserWidget = Cast<UUserWidget>(Widget))
	{
		if (UserWidget->WidgetTree)
		{
			bool bAnyTreeWidgetHovered = false;
			UserWidget->WidgetTree->ForEachWidget([this, Widget, &bAnyTreeWidgetHovered](UWidget* TreeWidget)
			{
				if (!bAnyTreeWidgetHovered && TreeWidget && TreeWidget != Widget)
				{
					bAnyTreeWidgetHovered = IsWidgetOrDescendantHovered(TreeWidget);
				}
			});
			if (bAnyTreeWidgetHovered)
			{
				return true;
			}
		}
	}

	return false;
}

bool UInventoryPanelWidget::ShouldUseCompareTooltipForItem(const FRetrieveItemStack& Item) const
{
	if (!InventoryComponent || !ItemCompareTooltipWidgetClass)
	{
		return false;
	}

	if (IsWeaponCategory(Item.ItemCategoryTag))
	{
		const FName EquippedWeaponId = InventoryComponent->GetEquippedWeaponId();
		return WeaponDataTable
			&& !EquippedWeaponId.IsNone()
			&& EquippedWeaponId != Item.ItemId;
	}

	if (IsArmorCategory(Item.ItemCategoryTag) && ArmorDataTable)
	{
		if (const FRetrieveArmorDataRow* Row = ArmorDataTable->FindRow<FRetrieveArmorDataRow>(
			Item.ItemId, TEXT("InventoryTooltip::ShouldCompareArmor"), false))
		{
			const FName EquippedArmorId = InventoryComponent->GetEquippedArmorId(Row->EquipmentSlotTag);
			return !EquippedArmorId.IsNone() && EquippedArmorId != Item.ItemId;
		}
	}

	return false;
}

FName UInventoryPanelWidget::GetCompareReferenceItemId(const FRetrieveItemStack& Item) const
{
	if (!InventoryComponent)
	{
		return NAME_None;
	}

	if (IsWeaponCategory(Item.ItemCategoryTag))
	{
		return InventoryComponent->GetEquippedWeaponId();
	}

	if (IsArmorCategory(Item.ItemCategoryTag) && ArmorDataTable)
	{
		if (const FRetrieveArmorDataRow* Row = ArmorDataTable->FindRow<FRetrieveArmorDataRow>(
			Item.ItemId, TEXT("InventoryTooltip::CompareReference"), false))
		{
			return InventoryComponent->GetEquippedArmorId(Row->EquipmentSlotTag);
		}
	}

	return NAME_None;
}

void UInventoryPanelWidget::PopulateFantasyTooltipWidget(
	UUserWidget* TooltipWidget,
	const FRetrieveItemStack& Item,
	const FString& BadgeText,
	const FString& OverrideMainStat,
	const FString& OverrideRarity,
	int32 OverrideBasePrice) const
{
	if (!TooltipWidget || Item.ItemId.IsNone())
	{
		return;
	}

	const int32 InventoryQuantity = InventoryComponent ? InventoryComponent->GetItemCount(Item.ItemId) : 0;
	const int32 Quantity = FMath::Max(Item.Quantity, InventoryQuantity);
	const FString TypeName = GetItemTypeName(Item);
	FString RarityText = !OverrideRarity.IsEmpty() ? OverrideRarity : TypeName;
	FString MainStatText = !OverrideMainStat.IsEmpty()
		? OverrideMainStat
		: (Quantity > 0 ? FString::Printf(TEXT("%d Owned"), Quantity) : TypeName);
	int32 BasePrice = OverrideBasePrice >= 0 ? OverrideBasePrice : 0;

	if (IsWeaponCategory(Item.ItemCategoryTag) && WeaponDataTable)
	{
		if (const FRetrieveWeaponDataRow* Row = WeaponDataTable->FindRow<FRetrieveWeaponDataRow>(Item.ItemId, TEXT("InventoryTooltip::PopulateWeapon")))
		{
			const FString GradeName = GetGameplayTagLeaf(Row->WeaponGradeTag);
			if (OverrideRarity.IsEmpty())
			{
				RarityText = GradeName.IsEmpty() ? TypeName : FString::Printf(TEXT("%s %s"), *GradeName, *TypeName);
			}
			if (OverrideMainStat.IsEmpty())
			{
				MainStatText = FString::Printf(TEXT("%.0f Attack"), Row->AttackPower);
			}
			if (OverrideBasePrice < 0)
			{
				BasePrice = Row->BasePrice;
			}
		}
	}
	else if (IsArmorCategory(Item.ItemCategoryTag) && ArmorDataTable)
	{
		if (const FRetrieveArmorDataRow* Row = ArmorDataTable->FindRow<FRetrieveArmorDataRow>(Item.ItemId, TEXT("InventoryTooltip::PopulateArmor")))
		{
			if (OverrideRarity.IsEmpty())
			{
				RarityText = FString::Printf(TEXT("Armor %s"), *GetGameplayTagLeaf(Row->EquipmentSlotTag));
			}
			if (OverrideMainStat.IsEmpty())
			{
				MainStatText = FString::Printf(TEXT("%.0f Defense"), Row->Defense);
			}
			if (OverrideBasePrice < 0)
			{
				BasePrice = Row->BasePrice;
			}
		}
	}
	else if (IsConsumableCategory(Item.ItemCategoryTag) && ConsumableItemTable)
	{
		if (const FRetrieveConsumableItemRow* Row = ConsumableItemTable->FindRow<FRetrieveConsumableItemRow>(Item.ItemId, TEXT("InventoryTooltip::PopulateConsumable")))
		{
			if (OverrideRarity.IsEmpty())
			{
				RarityText = TEXT("Consumable");
			}
			if (OverrideMainStat.IsEmpty())
			{
				MainStatText = Row->HealAmount > 0.0f
					? FString::Printf(TEXT("%.0f Heal"), Row->HealAmount)
					: FString::Printf(TEXT("%d Owned"), Quantity);
			}
			if (OverrideBasePrice < 0)
			{
				BasePrice = Row->BasePrice;
			}
		}
	}
	else if (IsMaterialCategory(Item.ItemCategoryTag))
	{
		if (UDataTable* Table = ResolveMaterialItemTable())
		{
			if (const FRetrieveMaterialItemRow* Row = Table->FindRow<FRetrieveMaterialItemRow>(Item.ItemId, TEXT("InventoryTooltip::PopulateMaterial")))
			{
				if (OverrideRarity.IsEmpty())
				{
					RarityText = TEXT("Material");
				}
				if (OverrideMainStat.IsEmpty())
				{
					MainStatText = FString::Printf(TEXT("%d Owned"), Quantity);
				}
				if (OverrideBasePrice < 0)
				{
					BasePrice = Row->BasePrice;
				}
			}
		}
	}

	SetTooltipText(TooltipWidget, TEXT("Text_Badge"), BadgeText, true);
	SetTooltipWidgetVisible(TooltipWidget, TEXT("IMG_BadgeFrame"), !BadgeText.IsEmpty());
	SetTooltipText(TooltipWidget, TEXT("Text_ItemName"), GetItemDisplayName(Item));
	SetTooltipText(TooltipWidget, TEXT("Text_ItemRarity"), RarityText);
	SetTooltipText(TooltipWidget, TEXT("Text_MainStat"), MainStatText);
	SetTooltipText(TooltipWidget, TEXT("Text_CurrencyValue"), FString::FromInt(BasePrice));

	FRetrieveItemIconRow IconData;
	if (GetItemIconData(Item.ItemId, IconData))
	{
		SetTooltipImage(TooltipWidget, TEXT("Image_ItemIcon"), IconData.IconTexture.LoadSynchronous());
	}
}

UWidget* UInventoryPanelWidget::CreateInventorySlotTooltip(const FRetrieveItemStack& Item)
{
	ResolveDefaultTooltipWidgetClasses();

	if (Item.ItemId.IsNone())
	{
		return nullptr;
	}

	const bool bUseCompareTooltip = ShouldUseCompareTooltipForItem(Item);
	TSubclassOf<UUserWidget> TooltipClass = bUseCompareTooltip
		? ItemCompareTooltipWidgetClass
		: ItemDetailTooltipWidgetClass;
	if (!TooltipClass)
	{
		return nullptr;
	}

	UUserWidget* TooltipWidget = CreateWidget<UUserWidget>(GetOwningPlayer(), TooltipClass);
	if (!TooltipWidget)
	{
		return nullptr;
	}

	TooltipWidget->SetToolTipText(FText::GetEmpty());
	TooltipWidget->SetToolTip(nullptr);

	FString BadgeText;
	if (IsWeaponCategory(Item.ItemCategoryTag) && IsWeaponItemEquipped(Item.ItemId, Item.SlotInstanceId))
	{
		BadgeText = TEXT("EQUIPPED");
	}
	else if (IsArmorCategory(Item.ItemCategoryTag) && ArmorDataTable && InventoryComponent)
	{
		if (const FRetrieveArmorDataRow* ArmorRow = ArmorDataTable->FindRow<FRetrieveArmorDataRow>(Item.ItemId, TEXT("InventoryTooltip::ArmorBadge")))
		{
			if (IsArmorItemEquipped(Item.ItemId, Item.SlotInstanceId))
			{
				BadgeText = TEXT("EQUIPPED");
			}
		}
	}

	if (bUseCompareTooltip && IsWeaponCategory(Item.ItemCategoryTag))
	{
		const FRetrieveWeaponDataRow* CurrentWeapon = WeaponDataTable->FindRow<FRetrieveWeaponDataRow>(
			InventoryComponent->GetEquippedWeaponId(),
			TEXT("InventoryTooltip::CurrentWeapon"),
			false);
		const FRetrieveWeaponDataRow* HoveredWeapon = WeaponDataTable->FindRow<FRetrieveWeaponDataRow>(
			Item.ItemId,
			TEXT("InventoryTooltip::HoveredWeapon"),
			false);

		if (CurrentWeapon && HoveredWeapon)
		{
			return CreateInventoryCompareTooltip(TooltipWidget, Item, *CurrentWeapon, *HoveredWeapon);
		}
	}
	else if (bUseCompareTooltip && IsArmorCategory(Item.ItemCategoryTag) && ArmorDataTable)
	{
		const FRetrieveArmorDataRow* HoveredArmor = ArmorDataTable->FindRow<FRetrieveArmorDataRow>(
			Item.ItemId,
			TEXT("InventoryTooltip::HoveredArmor"),
			false);
		const FRetrieveArmorDataRow* CurrentArmor = HoveredArmor
			? ArmorDataTable->FindRow<FRetrieveArmorDataRow>(
				InventoryComponent->GetEquippedArmorId(HoveredArmor->EquipmentSlotTag),
				TEXT("InventoryTooltip::CurrentArmor"),
				false)
			: nullptr;

		if (CurrentArmor && HoveredArmor)
		{
			return CreateInventoryCompareTooltip(TooltipWidget, Item, *CurrentArmor, *HoveredArmor);
		}
	}

	if (!bUseCompareTooltip)
	{
		InvokeTooltipTextFunction(
			TooltipWidget,
			TEXT("SetDetailInfo"),
			{
				GetItemDisplayName(Item),
				BuildItemTooltipText(Item.ItemId, Item.ItemCategoryTag).ToString()
			});
	}

	PopulateFantasyTooltipWidget(TooltipWidget, Item, BadgeText);
	return TooltipWidget;
}

UWidget* UInventoryPanelWidget::CreateInventoryCompareTooltip(
	UUserWidget* TooltipWidget,
	const FRetrieveItemStack& HoveredItem,
	const FRetrieveWeaponDataRow& CurrentWeapon,
	const FRetrieveWeaponDataRow& HoveredWeapon)
{
	if (!TooltipWidget)
	{
		return nullptr;
	}

	const FString CurrentInfo = FormatWeaponTooltipBlock(CurrentWeapon, TEXT("Current"));
	const FString HoveredInfo = FormatWeaponTooltipBlock(HoveredWeapon, TEXT("Selected"));
	const FString DeltaInfo = BuildWeaponSwapDeltaText(CurrentWeapon, HoveredWeapon);

	InvokeTooltipTextFunction(
		TooltipWidget,
		TEXT("SetCompareInfo"),
		{
			CurrentInfo,
			HoveredInfo,
			DeltaInfo
		});

	const FString GradeName = GetGameplayTagLeaf(HoveredWeapon.WeaponGradeTag);
	const FString TypeName = GetGameplayTagLeaf(HoveredWeapon.WeaponTypeTag);
	const FString RarityText = GradeName.IsEmpty() ? TypeName : FString::Printf(TEXT("%s %s"), *GradeName, *TypeName);

	SetTooltipText(TooltipWidget, TEXT("Text_CompareTitle"), TEXT("COMPARE"));
	SetTooltipText(TooltipWidget, TEXT("Text_ItemDetails"), DeltaInfo);
	PopulateFantasyTooltipWidget(
		TooltipWidget,
		HoveredItem,
		TEXT("COMPARE"),
		FString::Printf(TEXT("%.0f Attack"), HoveredWeapon.AttackPower),
		RarityText,
		HoveredWeapon.BasePrice);

	return TooltipWidget;

}

UWidget* UInventoryPanelWidget::CreateInventoryCompareTooltip(
	UUserWidget* TooltipWidget,
	const FRetrieveItemStack& HoveredItem,
	const FRetrieveArmorDataRow& CurrentArmor,
	const FRetrieveArmorDataRow& HoveredArmor)
{
	if (!TooltipWidget)
	{
		return nullptr;
	}

	const FString CurrentInfo = FormatArmorTooltipBlock(CurrentArmor, TEXT("Current"));
	const FString HoveredInfo = FormatArmorTooltipBlock(HoveredArmor, TEXT("Selected"));
	const FString DeltaInfo = BuildArmorSwapDeltaText(CurrentArmor, HoveredArmor);

	InvokeTooltipTextFunction(
		TooltipWidget,
		TEXT("SetCompareInfo"),
		{
			CurrentInfo,
			HoveredInfo,
			DeltaInfo
		});

	const FString SlotName = GetGameplayTagLeaf(HoveredArmor.EquipmentSlotTag);

	SetTooltipText(TooltipWidget, TEXT("Text_CompareTitle"), TEXT("COMPARE"));
	SetTooltipText(TooltipWidget, TEXT("Text_ItemDetails"), DeltaInfo);
	PopulateFantasyTooltipWidget(
		TooltipWidget,
		HoveredItem,
		TEXT("COMPARE"),
		FString::Printf(TEXT("%.0f Defense"), HoveredArmor.Defense),
		FString::Printf(TEXT("Armor %s"), *SlotName),
		HoveredArmor.BasePrice);

	return TooltipWidget;
}

UWidget* UInventoryPanelWidget::BuildEquipmentSlotTooltipWidget(const FRetrieveItemStack& Item) const
{
	if (Item.ItemId.IsNone() || !WidgetTree)
	{
		return nullptr;
	}

	USizeBox* RootBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	UVerticalBox* LineBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (!RootBox || !Background || !LineBox)
	{
		return nullptr;
	}

	RootBox->SetWidthOverride(280.0f);
	RootBox->SetContent(Background);
	Background->SetPadding(FMargin(8.0f, 7.0f));
	Background->SetBrushColor(FLinearColor(0.015f, 0.018f, 0.03f, 0.92f));
	Background->SetContent(LineBox);

	const FLinearColor TitleColor(1.0f, 0.86f, 0.38f, 1.0f);
	const FLinearColor BodyColor(0.92f, 0.92f, 0.86f, 1.0f);
	const FLinearColor MutedColor(0.68f, 0.68f, 0.64f, 1.0f);

	AddTooltipTextLine(LineBox, GetItemDisplayName(Item), TitleColor, true);

	const FString TooltipBody = BuildItemTooltipText(Item.ItemId, Item.ItemCategoryTag).ToString();
	if (!TooltipBody.IsEmpty())
	{
		TArray<FString> Lines;
		TooltipBody.ParseIntoArrayLines(Lines, false);
		for (const FString& Line : Lines)
		{
			if (Line.IsEmpty()) continue;
			const bool bIsSeparator = Line.StartsWith(TEXT("──"));
			AddTooltipTextLine(LineBox, Line, bIsSeparator ? MutedColor : BodyColor, false);
		}
	}

	return RootBox;
}

void UInventoryPanelWidget::AddTooltipTextLine(
	UVerticalBox* LineBox,
	const FString& Line,
	const FLinearColor& Color,
	bool bHeading) const
{
	if (!LineBox)
	{
		return;
	}

	UTextBlock* TextLine = WidgetTree ? WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass()) : nullptr;
	if (!TextLine)
	{
		return;
	}

	FSlateFontInfo FontInfo = TextLine->GetFont();
	FontInfo.Size = bHeading ? 13 : 11;
	TextLine->SetFont(FontInfo);
	TextLine->SetText(FText::FromString(Line));
	TextLine->SetColorAndOpacity(FSlateColor(Color));
	TextLine->SetAutoWrapText(true);
	TextLine->SetWrapTextAt(244.0f);
	TextLine->SetShadowOffset(FVector2D(1.0f, 1.0f));
	TextLine->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.75f));

	if (UVerticalBoxSlot* LineSlot = LineBox->AddChildToVerticalBox(TextLine))
	{
		LineSlot->SetPadding(FMargin(0.0f, bHeading ? 2.0f : 0.0f, 0.0f, 0.0f));
	}
}

void UInventoryPanelWidget::InvokeTooltipTextFunction(
	UUserWidget* TooltipWidget,
	FName FunctionName,
	const TArray<FString>& Values) const
{
	if (!TooltipWidget)
	{
		return;
	}

	UFunction* Function = TooltipWidget->FindFunction(FunctionName);
	if (!Function || Function->ParmsSize <= 0)
	{
		return;
	}

	TArray<uint8> Params;
	Params.SetNumZeroed(Function->ParmsSize);

	for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		if (FProperty* Property = *It)
		{
			Property->InitializeValue_InContainer(Params.GetData());
		}
	}

	int32 ValueIndex = 0;
	for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		FProperty* Property = *It;
		if (!Property || Property->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			continue;
		}

		const FString& Value = Values.IsValidIndex(ValueIndex) ? Values[ValueIndex] : FString();
		void* ParamValue = Property->ContainerPtrToValuePtr<void>(Params.GetData());

		if (FStrProperty* StringProperty = CastField<FStrProperty>(Property))
		{
			StringProperty->SetPropertyValue(ParamValue, Value);
			++ValueIndex;
		}
		else if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
		{
			TextProperty->SetPropertyValue(ParamValue, FText::FromString(Value));
			++ValueIndex;
		}
	}

	TooltipWidget->ProcessEvent(Function, Params.GetData());

	for (TFieldIterator<FProperty> It(Function); It && It->HasAnyPropertyFlags(CPF_Parm); ++It)
	{
		if (FProperty* Property = *It)
		{
			Property->DestroyValue_InContainer(Params.GetData());
		}
	}
}

FString UInventoryPanelWidget::FormatWeaponTooltipBlock(
	const FRetrieveWeaponDataRow& WeaponData,
	const FString& Header) const
{
	TArray<FString> Lines;
	Lines.Add(Header + TEXT(":"));
	Lines.Add(WeaponData.DisplayName.ToString());
	Lines.Add(FString::Printf(TEXT("Grade: %s"), *GetGameplayTagLeaf(WeaponData.WeaponGradeTag)));
	Lines.Add(FString::Printf(TEXT("Type: %s"), *GetGameplayTagLeaf(WeaponData.WeaponTypeTag)));
	Lines.Add(FString::Printf(TEXT("Element: %s"), *GetGameplayTagLeaf(WeaponData.WeaponAffinityTag)));
	Lines.Add(FString::Printf(TEXT("Attack Power: %.0f"), WeaponData.AttackPower));
	Lines.Add(FString::Printf(TEXT("Element Charge: x%.2f"), WeaponData.ElementChargeMultiplier));

	// ShortDescription은 길이가 가변적이라 비교 칸(고정 크기)에 넣으면 넘치기 쉽다.
	// 상세 설명은 BuildItemTooltipText 쪽 일반 툴팁에서만 보여준다.

	return FString::Join(Lines, TEXT("\n"));
}

FString UInventoryPanelWidget::BuildWeaponSwapDeltaText(
	const FRetrieveWeaponDataRow& CurrentWeapon,
	const FRetrieveWeaponDataRow& HoveredWeapon) const
{
	TArray<FString> Lines;

	const float AttackDelta = HoveredWeapon.AttackPower - CurrentWeapon.AttackPower;
	if (!FMath::IsNearlyZero(AttackDelta))
	{
		Lines.Add(FString::Printf(TEXT("%+.0f Attack Power"), AttackDelta));
	}

	const float ElementChargeDelta = HoveredWeapon.ElementChargeMultiplier - CurrentWeapon.ElementChargeMultiplier;
	if (!FMath::IsNearlyZero(ElementChargeDelta))
	{
		Lines.Add(FString::Printf(TEXT("%+.2f Element Charge"), ElementChargeDelta));
	}

	if (HoveredWeapon.WeaponTypeTag != CurrentWeapon.WeaponTypeTag)
	{
		Lines.Add(FString::Printf(
			TEXT("Type: %s -> %s"),
			*GetGameplayTagLeaf(CurrentWeapon.WeaponTypeTag),
			*GetGameplayTagLeaf(HoveredWeapon.WeaponTypeTag)));
	}

	if (HoveredWeapon.WeaponAffinityTag != CurrentWeapon.WeaponAffinityTag)
	{
		Lines.Add(FString::Printf(
			TEXT("Element: %s -> %s"),
			*GetGameplayTagLeaf(CurrentWeapon.WeaponAffinityTag),
			*GetGameplayTagLeaf(HoveredWeapon.WeaponAffinityTag)));
	}

	if (Lines.IsEmpty())
	{
		Lines.Add(TEXT("No stat changes"));
	}

	return FString::Join(Lines, TEXT("\n"));
}

FString UInventoryPanelWidget::FormatArmorTooltipBlock(
	const FRetrieveArmorDataRow& ArmorData,
	const FString& Header) const
{
	TArray<FString> Lines;
	Lines.Add(Header + TEXT(":"));
	Lines.Add(ArmorData.DisplayName.ToString());
	Lines.Add(FString::Printf(TEXT("Slot: %s"), *GetGameplayTagLeaf(ArmorData.EquipmentSlotTag)));
	Lines.Add(FString::Printf(TEXT("Defense: %.0f"), ArmorData.Defense));

	// ShortDescription은 길이가 가변적이라 비교 칸(고정 크기)에 넣으면 넘치기 쉽다.
	// 상세 설명은 BuildItemTooltipText 쪽 일반 툴팁에서만 보여준다.

	return FString::Join(Lines, TEXT("\n"));
}

FString UInventoryPanelWidget::BuildArmorSwapDeltaText(
	const FRetrieveArmorDataRow& CurrentArmor,
	const FRetrieveArmorDataRow& HoveredArmor) const
{
	TArray<FString> Lines;

	const float DefenseDelta = HoveredArmor.Defense - CurrentArmor.Defense;
	if (!FMath::IsNearlyZero(DefenseDelta))
	{
		Lines.Add(FString::Printf(TEXT("%+.0f Defense"), DefenseDelta));
	}

	if (Lines.IsEmpty())
	{
		Lines.Add(TEXT("No stat changes"));
	}

	return FString::Join(Lines, TEXT("\n"));
}

void UInventoryPanelWidget::RefreshWeaponSkillIcons()
{
	FRetrieveWeaponDataRow CurrentWeaponData;
	PopulateWeaponSkillIcons(
		HorizontalBox_CurrentWeaponSkillIcons,
		GetCurrentWeaponData(CurrentWeaponData) ? CurrentWeaponData.SkillPreviews : TArray<FWeaponSkillPreview>());

	FRetrieveWeaponDataRow SelectedWeaponData;
	PopulateWeaponSkillIcons(
		HorizontalBox_SelectedWeaponSkillIcons,
		GetSelectedWeaponData(SelectedWeaponData) ? SelectedWeaponData.SkillPreviews : TArray<FWeaponSkillPreview>());
}

void UInventoryPanelWidget::PopulateWeaponSkillIcons(UHorizontalBox* SkillIconBox, const TArray<FWeaponSkillPreview>& SkillPreviews) const
{
	if (!SkillIconBox || !WidgetTree)
	{
		return;
	}

	SkillIconBox->ClearChildren();

	const int32 IconCount = FMath::Max(SkillPreviews.Num(), 2);
	for (int32 Index = 0; Index < IconCount; ++Index)
	{
		USizeBox* SkillIconFrame = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		if (!SkillIconFrame)
		{
			continue;
		}

		SkillIconFrame->SetWidthOverride(28.0f);
		SkillIconFrame->SetHeightOverride(28.0f);

		UImage* SkillIcon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		if (!SkillIcon)
		{
			continue;
		}

		SkillIcon->SetDesiredSizeOverride(FVector2D(24.0f, 24.0f));
		SkillIcon->SetColorAndOpacity(FLinearColor(0.18f, 0.18f, 0.16f, 0.85f));

		if (SkillPreviews.IsValidIndex(Index))
		{
			const FWeaponSkillPreview& SkillPreview = SkillPreviews[Index];
			if (UTexture2D* IconTexture = SkillPreview.Icon.LoadSynchronous())
			{
				SkillIcon->SetBrushFromTexture(IconTexture, false);
				SkillIcon->SetColorAndOpacity(FLinearColor::White);
			}

			const FText SkillName = SkillPreview.DisplayName.IsEmpty()
				? FText::FromString(GetGameplayTagLeaf(SkillPreview.AbilityTag))
				: SkillPreview.DisplayName;
			SkillIcon->SetToolTipText(SkillPreview.ShortDescription.IsEmpty() ? SkillName : SkillPreview.ShortDescription);
		}
		else
		{
			SkillIcon->SetToolTipText(FText::FromString(TEXT("Empty Skill Slot")));
		}

		SkillIconFrame->AddChild(SkillIcon);
		if (UHorizontalBoxSlot* IconSlot = SkillIconBox->AddChildToHorizontalBox(SkillIconFrame))
		{
			IconSlot->SetPadding(FMargin(0.0f, 0.0f, 6.0f, 0.0f));
		}
	}
}

FString UInventoryPanelWidget::BuildWeaponComparisonText() const
{
	FRetrieveWeaponDataRow SelectedWeaponData;
	const bool bHasSelectedWeapon = GetSelectedWeaponData(SelectedWeaponData);

	FRetrieveWeaponDataRow CurrentWeaponData;
	const bool bHasCurrentWeapon = GetCurrentWeaponData(CurrentWeaponData);

	if (!bHasSelectedWeapon && !bHasCurrentWeapon)
	{
		return TEXT("Compare\nSelect a weapon.");
	}

	if (!bHasSelectedWeapon)
	{
		return FString::Printf(TEXT("Current\n%s\n\nSelected\nNone"), *FormatWeaponSummary(CurrentWeaponData));
	}

	if (!bHasCurrentWeapon)
	{
		return FString::Printf(TEXT("Selected\n%s\n\nCurrent\nNone"), *FormatWeaponSummary(SelectedWeaponData));
	}

	const float AttackDelta = SelectedWeaponData.AttackPower - CurrentWeaponData.AttackPower;
	const FString AttackDeltaText = AttackDelta >= 0.0f
		? FString::Printf(TEXT("+%.0f"), AttackDelta)
		: FString::Printf(TEXT("%.0f"), AttackDelta);

	return FString::Printf(
		TEXT("Current -> Selected\n")
		TEXT("%s -> %s\n")
		TEXT("ATK %.0f -> %.0f (%s)\n")
		TEXT("Type %s -> %s\n")
		TEXT("Element %s -> %s"),
		*CurrentWeaponData.DisplayName.ToString(),
		*SelectedWeaponData.DisplayName.ToString(),
		CurrentWeaponData.AttackPower,
		SelectedWeaponData.AttackPower,
		*AttackDeltaText,
		*GetGameplayTagLeaf(CurrentWeaponData.WeaponTypeTag),
		*GetGameplayTagLeaf(SelectedWeaponData.WeaponTypeTag),
		*GetGameplayTagLeaf(CurrentWeaponData.WeaponAffinityTag),
		*GetGameplayTagLeaf(SelectedWeaponData.WeaponAffinityTag));
}

FString UInventoryPanelWidget::FormatWeaponSummary(const FRetrieveWeaponDataRow& WeaponData) const
{
	return FString::Printf(
		TEXT("%s\nATK %.0f\nType %s\nElement %s"),
		*WeaponData.DisplayName.ToString(),
		WeaponData.AttackPower,
		*GetGameplayTagLeaf(WeaponData.WeaponTypeTag),
		*GetGameplayTagLeaf(WeaponData.WeaponAffinityTag));
}

FString UInventoryPanelWidget::FormatWeaponSkillList(const FRetrieveWeaponDataRow& WeaponData) const
{
	TArray<FString> SkillLines;
	for (const FWeaponSkillPreview& SkillPreview : WeaponData.SkillPreviews)
	{
		FString SkillName = SkillPreview.DisplayName.ToString();
		if (SkillName.IsEmpty() && SkillPreview.AbilityTag.IsValid())
		{
			SkillName = GetGameplayTagLeaf(SkillPreview.AbilityTag);
		}

		if (SkillName.IsEmpty())
		{
			continue;
		}

		const FString Description = SkillPreview.ShortDescription.ToString();
		SkillLines.Add(FString::Printf(TEXT("- %s"), *SkillName));
	}

	if (SkillLines.IsEmpty())
	{
		for (const FGameplayTag& AbilityTag : WeaponData.GrantedAbilityTags)
		{
			if (AbilityTag.IsValid())
			{
				SkillLines.Add(FString::Printf(TEXT("- %s"), *GetGameplayTagLeaf(AbilityTag)));
			}
		}
	}

	return SkillLines.IsEmpty()
		? FString(TEXT("- None"))
		: FString::Join(SkillLines, TEXT("\n"));
}

bool UInventoryPanelWidget::CanProcessSelectedItemActivation()
{
	const double CurrentTime = FPlatformTime::Seconds();
	const bool bSameActivation =
		LastActivatedItemId == SelectedItemId &&
		LastActivatedItemCategoryTag == SelectedItemCategoryTag &&
		LastActivatedSlotInstanceId == SelectedSlotInstanceId;

	if (bSameActivation
		&& LastSelectedItemActivationTime >= 0.0
		&& CurrentTime - LastSelectedItemActivationTime < SelectedItemActivationGuardSeconds)
	{
		return false;
	}

	LastSelectedItemActivationTime = CurrentTime;
	LastActivatedItemId = SelectedItemId;
	LastActivatedItemCategoryTag = SelectedItemCategoryTag;
	LastActivatedSlotInstanceId = SelectedSlotInstanceId;
	return true;
}

FString UInventoryPanelWidget::GetGameplayTagLeaf(FGameplayTag Tag)
{
	if (!Tag.IsValid())
	{
		return TEXT("None");
	}

	FString TagString = Tag.GetTagName().ToString();
	int32 LastDotIndex = INDEX_NONE;
	if (TagString.FindLastChar(TEXT('.'), LastDotIndex))
	{
		TagString = TagString.RightChop(LastDotIndex + 1);
	}
	return TagString;
}

// ─────────────────────────────────────────────────────────────────────────────
// 정렬
// ─────────────────────────────────────────────────────────────────────────────

void UInventoryPanelWidget::SetSortMode(EInventorySortMode NewMode)
{
	CurrentSortMode = NewMode;
	MarkInventoryTooltipsDirty();
	// 목록 갱신 이벤트를 발행하면 BP가 GetCurrentItemsSorted()를 다시 호출해 UI를 재구성한다
	OnInventoryListChanged.Broadcast();
}

void UInventoryPanelWidget::CycleSortByName()
{
	if (CurrentSortMode == EInventorySortMode::NameAsc)
	{
		SetSortMode(EInventorySortMode::NameDesc);
	}
	else if (CurrentSortMode == EInventorySortMode::NameDesc)
	{
		SetSortMode(EInventorySortMode::None);
	}
	else
	{
		SetSortMode(EInventorySortMode::NameAsc);
	}
}

void UInventoryPanelWidget::CycleSortByType()
{
	if (CurrentSortMode == EInventorySortMode::TypeAsc)
	{
		SetSortMode(EInventorySortMode::TypeDesc);
	}
	else if (CurrentSortMode == EInventorySortMode::TypeDesc)
	{
		SetSortMode(EInventorySortMode::None);
	}
	else
	{
		SetSortMode(EInventorySortMode::TypeAsc);
	}
}

void UInventoryPanelWidget::CycleSortByAttackPower()
{
	// 무기 탭이 아닐 때는 공격력 정렬을 허용하지 않는다
	if (!IsWeaponCategory(CurrentCategoryTag))
	{
		return;
	}

	if (CurrentSortMode == EInventorySortMode::AttackPowerAsc)
	{
		SetSortMode(EInventorySortMode::AttackPowerDesc);
	}
	else if (CurrentSortMode == EInventorySortMode::AttackPowerDesc)
	{
		SetSortMode(EInventorySortMode::None);
	}
	else
	{
		SetSortMode(EInventorySortMode::AttackPowerAsc);
	}
}

TArray<FRetrieveItemStack> UInventoryPanelWidget::GetCurrentItemsSorted() const
{
	TArray<FRetrieveItemStack> Items = GetCurrentItems();
	SortItemStacks(Items);
	return Items;
}

ESlateVisibility UInventoryPanelWidget::GetSortAttackPowerButtonVisibility() const
{
	return IsWeaponCategory(CurrentCategoryTag) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
}

void UInventoryPanelWidget::CycleSortByDefense()
{
	if (!IsArmorCategory(CurrentCategoryTag))
	{
		return;
	}

	if (CurrentSortMode == EInventorySortMode::DefenseAsc)
	{
		SetSortMode(EInventorySortMode::DefenseDesc);
	}
	else if (CurrentSortMode == EInventorySortMode::DefenseDesc)
	{
		SetSortMode(EInventorySortMode::None);
	}
	else
	{
		SetSortMode(EInventorySortMode::DefenseAsc);
	}
}

ESlateVisibility UInventoryPanelWidget::GetSortDefenseButtonVisibility() const
{
	return IsArmorCategory(CurrentCategoryTag) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
}

void UInventoryPanelWidget::SortItemStacks(TArray<FRetrieveItemStack>& Items) const
{
	if (CurrentSortMode == EInventorySortMode::None || Items.Num() < 2)
	{
		return;
	}

	Items.StableSort([this](const FRetrieveItemStack& A, const FRetrieveItemStack& B)
	{
		switch (CurrentSortMode)
		{
		case EInventorySortMode::NameAsc:
			return GetItemDisplayName(A) < GetItemDisplayName(B);

		case EInventorySortMode::NameDesc:
			return GetItemDisplayName(A) > GetItemDisplayName(B);

		case EInventorySortMode::TypeAsc:
			return GetItemTypeName(A) < GetItemTypeName(B);

		case EInventorySortMode::TypeDesc:
			return GetItemTypeName(A) > GetItemTypeName(B);

		case EInventorySortMode::AttackPowerAsc:
			return GetItemAttackPower(A) < GetItemAttackPower(B);

		case EInventorySortMode::AttackPowerDesc:
			return GetItemAttackPower(A) > GetItemAttackPower(B);

		case EInventorySortMode::DefenseAsc:
			return GetItemDefensePower(A) < GetItemDefensePower(B);

		case EInventorySortMode::DefenseDesc:
			return GetItemDefensePower(A) > GetItemDefensePower(B);

		default:
			return false;
		}
	});
}

FString UInventoryPanelWidget::GetItemDisplayName(const FRetrieveItemStack& Item) const
{
	if (IsWeaponCategory(Item.ItemCategoryTag) && WeaponDataTable)
	{
		if (const FRetrieveWeaponDataRow* Row = WeaponDataTable->FindRow<FRetrieveWeaponDataRow>(
			Item.ItemId, TEXT("")))
		{
			return Row->DisplayName.ToString();
		}
	}
	else if (IsConsumableCategory(Item.ItemCategoryTag) && ConsumableItemTable)
	{
		if (const FRetrieveConsumableItemRow* Row = ConsumableItemTable->FindRow<FRetrieveConsumableItemRow>(
			Item.ItemId, TEXT("")))
		{
			return Row->DisplayName.ToString();
		}
	}
	else if (IsMaterialCategory(Item.ItemCategoryTag))
	{
		if (UDataTable* Table = ResolveMaterialItemTable())
		{
			if (const FRetrieveMaterialItemRow* Row = Table->FindRow<FRetrieveMaterialItemRow>(
				Item.ItemId, TEXT("")))
			{
				return Row->DisplayName.ToString();
			}
		}
	}
	else if (IsArmorCategory(Item.ItemCategoryTag) && ArmorDataTable)
	{
		if (const FRetrieveArmorDataRow* Row = ArmorDataTable->FindRow<FRetrieveArmorDataRow>(
			Item.ItemId, TEXT("")))
		{
			return Row->DisplayName.ToString();
		}
	}
	// 테이블 미등록 시 ItemId 문자열로 폴백
	return Item.ItemId.ToString();
}

FString UInventoryPanelWidget::GetItemTypeName(const FRetrieveItemStack& Item) const
{
	// 무기는 WeaponTypeTag(검/쌍검/스태프 등), 방어구는 EquipmentSlotTag 말단, 그 외는 카테고리 태그 말단
	if (IsWeaponCategory(Item.ItemCategoryTag) && WeaponDataTable)
	{
		if (const FRetrieveWeaponDataRow* Row = WeaponDataTable->FindRow<FRetrieveWeaponDataRow>(
			Item.ItemId, TEXT("")))
		{
			return GetGameplayTagLeaf(Row->WeaponTypeTag);
		}
	}
	if (IsArmorCategory(Item.ItemCategoryTag) && ArmorDataTable)
	{
		if (const FRetrieveArmorDataRow* Row = ArmorDataTable->FindRow<FRetrieveArmorDataRow>(
			Item.ItemId, TEXT("")))
		{
			return GetGameplayTagLeaf(Row->EquipmentSlotTag);
		}
	}
	return GetGameplayTagLeaf(Item.ItemCategoryTag);
}

float UInventoryPanelWidget::GetItemAttackPower(const FRetrieveItemStack& Item) const
{
	if (IsWeaponCategory(Item.ItemCategoryTag) && WeaponDataTable)
	{
		if (const FRetrieveWeaponDataRow* Row = WeaponDataTable->FindRow<FRetrieveWeaponDataRow>(
			Item.ItemId, TEXT("")))
		{
			return Row->AttackPower;
		}
	}
	return 0.0f;
}

float UInventoryPanelWidget::GetItemDefensePower(const FRetrieveItemStack& Item) const
{
	if (IsArmorCategory(Item.ItemCategoryTag) && ArmorDataTable)
	{
		if (const FRetrieveArmorDataRow* Row = ArmorDataTable->FindRow<FRetrieveArmorDataRow>(
			Item.ItemId, TEXT("")))
		{
			return Row->Defense;
		}
	}
	return 0.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
// 최종 스탯 조회
// ─────────────────────────────────────────────────────────────────────────────

URetrieveAbilitySystemComponent* UInventoryPanelWidget::GetOwnerASC() const
{
	const APawn* OwningPawn = GetOwningPlayerPawn();
	if (!OwningPawn)
	{
		return nullptr;
	}
	const URetrievePawnExtensionComponent* PawnExt =
		URetrievePawnExtensionComponent::FindPawnExtensionComponent(OwningPawn);
	return PawnExt ? PawnExt->GetRetrieveAbilitySystemComponent() : nullptr;
}

namespace
{
	// CanChangeEquipment()를 false로 만드는 상태 태그들 — 이 태그가 바뀌면 버튼 활성 상태를 갱신해야 한다.
	static const FGameplayTag* GetEquipLockTags(int32& OutNum)
	{
		static const FGameplayTag LockTags[] = {
			RetrieveGameplayTags::State_Player_Combat,
			RetrieveGameplayTags::State_Player_Dodging,
			RetrieveGameplayTags::State_Player_Guarding,
			RetrieveGameplayTags::State_Player_Parrying,
			RetrieveGameplayTags::State_Player_Bursting,
			RetrieveGameplayTags::State_Player_Staggered,
		};
		OutNum = UE_ARRAY_COUNT(LockTags);
		return LockTags;
	}
}

void UInventoryPanelWidget::BindEquipLockTagEvents()
{
	URetrieveAbilitySystemComponent* ASC = GetOwnerASC();
	if (BoundEquipLockASC.Get() == ASC)
	{
		// 이미 같은 ASC에 구독돼 있으면 중복 구독하지 않는다
		return;
	}
	UnbindEquipLockTagEvents();
	if (!ASC)
	{
		return;
	}

	int32 NumTags = 0;
	const FGameplayTag* LockTags = GetEquipLockTags(NumTags);
	for (int32 Index = 0; Index < NumTags; ++Index)
	{
		const FDelegateHandle Handle = ASC->RegisterGameplayTagEvent(LockTags[Index], EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ThisClass::HandleEquipLockTagChanged);
		EquipLockTagHandles.Add(Handle);
	}
	BoundEquipLockASC = ASC;
}

void UInventoryPanelWidget::UnbindEquipLockTagEvents()
{
	if (URetrieveAbilitySystemComponent* ASC = BoundEquipLockASC.Get())
	{
		int32 NumTags = 0;
		const FGameplayTag* LockTags = GetEquipLockTags(NumTags);
		const int32 Count = FMath::Min<int32>(NumTags, EquipLockTagHandles.Num());
		for (int32 Index = 0; Index < Count; ++Index)
		{
			ASC->RegisterGameplayTagEvent(LockTags[Index], EGameplayTagEventType::NewOrRemoved)
				.Remove(EquipLockTagHandles[Index]);
		}
	}
	EquipLockTagHandles.Reset();
	BoundEquipLockASC.Reset();
}

void UInventoryPanelWidget::HandleEquipLockTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	// 전투/회피/가드 등 잠금 상태가 바뀌면 장착/해제 버튼의 활성 상태를 즉시 재평가한다
	// (장착 직후 발검으로 잠겼다가 풀릴 때 재선택 없이 자동으로 버튼이 활성화되도록)
	UpdateEquipActionButtons();
}

float UInventoryPanelWidget::GetCharacterBaseAttackPower() const
{
	const URetrieveAbilitySystemComponent* ASC = GetOwnerASC();
	if (!ASC)
	{
		return 0.0f;
	}
	// GetNumericAttributeBase: Modifier(무기 GE 등) 적용 전 순수 Base 값
	return ASC->GetNumericAttributeBase(UCombatAttributeSet::GetAttackPowerAttribute());
}

float UInventoryPanelWidget::GetWeaponBonusAttackPower() const
{
	FRetrieveWeaponDataRow CurrentWeapon;
	return GetCurrentWeaponData(CurrentWeapon) ? CurrentWeapon.AttackPower : 0.0f;
}

float UInventoryPanelWidget::GetTotalAttackPower() const
{
	const URetrieveAbilitySystemComponent* ASC = GetOwnerASC();
	if (!ASC)
	{
		return 0.0f;
	}
	// GetNumericAttribute: 모든 Modifier(무기 GE 포함) 적용 후 최종 값
	return ASC->GetNumericAttribute(UCombatAttributeSet::GetAttackPowerAttribute());
}

float UInventoryPanelWidget::GetCharacterBaseDefense() const
{
	const URetrieveAbilitySystemComponent* ASC = GetOwnerASC();
	return ASC ? ASC->GetNumericAttributeBase(UCombatAttributeSet::GetDefenseAttribute()) : 0.0f;
}

float UInventoryPanelWidget::GetArmorBonusDefense() const
{
	if (!InventoryComponent || !ArmorDataTable)
	{
		return 0.0f;
	}

	float TotalDefense = 0.0f;
	for (const FRetrieveEquippedArmorEntry& ArmorSlot : InventoryComponent->GetEquippedArmorSlots())
	{
		if (ArmorSlot.ArmorItemId.IsNone())
		{
			continue;
		}

		const FRetrieveArmorDataRow* ArmorData = ArmorDataTable->FindRow<FRetrieveArmorDataRow>(
			ArmorSlot.ArmorItemId,
			TEXT("UInventoryPanelWidget::GetArmorBonusDefense"));
		if (ArmorData)
		{
			TotalDefense += ArmorData->Defense;
		}
	}

	return TotalDefense;
}

float UInventoryPanelWidget::GetTotalDefense() const
{
	const URetrieveAbilitySystemComponent* ASC = GetOwnerASC();
	const float AttributeDefense = ASC
		? ASC->GetNumericAttribute(UCombatAttributeSet::GetDefenseAttribute())
		: 0.0f;
	const float DataTableDefense = GetCharacterBaseDefense() + GetArmorBonusDefense();
	return FMath::Max(AttributeDefense, DataTableDefense);
}

float UInventoryPanelWidget::GetTotalMaxHealth() const
{
	const URetrieveAbilitySystemComponent* ASC = GetOwnerASC();
	return ASC ? ASC->GetNumericAttribute(UCombatAttributeSet::GetMaxHealthAttribute()) : 0.0f;
}

float UInventoryPanelWidget::GetTotalMoveSpeed() const
{
	const URetrieveAbilitySystemComponent* ASC = GetOwnerASC();
	return ASC ? ASC->GetNumericAttribute(UCombatAttributeSet::GetMoveSpeedAttribute()) : 0.0f;
}

void UInventoryPanelWidget::RefreshStatDisplay()
{
	if (Txt_StatAtkValue)
	{
		Txt_StatAtkValue->SetText(FText::AsNumber(FMath::RoundToInt(GetTotalAttackPower())));
	}
	if (Txt_StatDefValue)
	{
		Txt_StatDefValue->SetText(FText::AsNumber(FMath::RoundToInt(GetTotalDefense())));
	}
	if (Txt_StatHpValue)
	{
		Txt_StatHpValue->SetText(FText::AsNumber(FMath::RoundToInt(GetTotalMaxHealth())));
	}
	if (Txt_StatSpdValue)
	{
		Txt_StatSpdValue->SetText(FText::AsNumber(FMath::RoundToInt(GetTotalMoveSpeed())));
	}
}

FText UInventoryPanelWidget::GetFinalStatDisplayText() const
{
	const float BaseATK   = GetCharacterBaseAttackPower();
	const float WeaponATK = GetWeaponBonusAttackPower();
	const float TotalATK  = GetTotalAttackPower();
	const float BaseDEF   = GetCharacterBaseDefense();
	const float ArmorDEF  = GetArmorBonusDefense();
	const float TotalDEF  = GetTotalDefense();

	if (const UWorld* World = GetWorld(); World || !World)
	{
		return FText::FromString(FString::Printf(
		TEXT("Base ATK: %.0f\nWeapon ATK: +%.0f\nTotal ATK: %.0f\n기본 방어력: %.0f\n방어구 방어력: +%.0f\n최종 방어력: %.0f"),
		BaseATK, WeaponATK, TotalATK, BaseDEF, ArmorDEF, TotalDEF));
	}

	const FString DisplayStr = FString::Printf(
		TEXT("기본 ATK: %.0f\n무기 보너스: +%.0f\n최종 ATK: %.0f"),
		BaseATK, WeaponATK, TotalATK);

	return FText::FromString(DisplayStr);
}

FText UInventoryPanelWidget::GetFullStatDisplayText() const
{
	const URetrieveAbilitySystemComponent* ASC = GetOwnerASC();

	TArray<FString> Lines;

	// 현재 체력 / 최대 체력 (ASC 없으면 0으로 표시해 포맷 통일)
	const float HP    = ASC ? ASC->GetNumericAttribute(UCombatAttributeSet::GetHealthAttribute())    : 0.f;
	const float MaxHP = ASC ? ASC->GetNumericAttribute(UCombatAttributeSet::GetMaxHealthAttribute()) : 0.f;
	Lines.Add(FString::Printf(TEXT("현재 체력: %.0f / %.0f"), HP, MaxHP));

	// 공격력 분류 (순수 함수들은 ASC 내부에서 개별 null 처리함)
	Lines.Add(FString::Printf(TEXT("기본 공격력: %.0f"), GetCharacterBaseAttackPower()));
	Lines.Add(FString::Printf(TEXT("무기 공격력: +%.0f"), GetWeaponBonusAttackPower()));
	Lines.Add(FString::Printf(TEXT("최종 공격력: %.0f"), GetTotalAttackPower()));

	// 방어력 (ArmorComponent가 GE로 CombatAttributeSet::Defense를 가산)
	const float Defense = GetTotalDefense();
	Lines.Add(FString::Printf(TEXT("기본 방어력: %.0f"), GetCharacterBaseDefense()));
	Lines.Add(FString::Printf(TEXT("방어구 방어력: +%.0f"), GetArmorBonusDefense()));
	Lines.Add(FString::Printf(TEXT("최종 방어력: %.0f"), Defense));

	// 체력/공격력/방어력 외의 DT_CharacterStats 추가 컬럼(MoveSpeed, MaxStamina 등)은
	// 인벤토리 최종 스탯 표시에 불필요하므로 자동 표시하지 않는다.

	return FText::FromString(FString::Join(Lines, TEXT("\n")));
}

// ─────────────────────────────────────────────────────────────────────────────
// 장착 프리뷰 슬롯 버튼 툴팁 갱신
// ─────────────────────────────────────────────────────────────────────────────

void UInventoryPanelWidget::RefreshSlotButtonTooltips()
{
	if (!InventoryComponent)
	{
		return;
	}

	auto SetArmorSlotTooltip = [this](UButton* Button, FGameplayTag SlotTag)
	{
		if (!Button)
		{
			return;
		}
		const FName ArmorId = InventoryComponent->GetEquippedArmorId(SlotTag);
		if (ArmorId.IsNone())
		{
			Button->SetToolTip(nullptr);
			Button->SetToolTipText(FText::GetEmpty());
		}
		else
		{
			FRetrieveItemStack ItemStack;
			ItemStack.ItemId = ArmorId;
			ItemStack.ItemCategoryTag = RetrieveGameplayTags::Item_Armor;
			ItemStack.Quantity = 1;
			Button->SetToolTip(BuildEquipmentSlotTooltipWidget(ItemStack));
		}
	};

	SetArmorSlotTooltip(Button_SlotHead,    RetrieveGameplayTags::Equipment_Slot_Head);
	SetArmorSlotTooltip(Button_SlotChest,   RetrieveGameplayTags::Equipment_Slot_Chest);
	SetArmorSlotTooltip(Button_SlotHands_L, RetrieveGameplayTags::Equipment_Slot_Hands);
	SetArmorSlotTooltip(Button_SlotHands_R, RetrieveGameplayTags::Equipment_Slot_Hands);
	SetArmorSlotTooltip(Button_SlotLegs,    RetrieveGameplayTags::Equipment_Slot_Legs);
	SetArmorSlotTooltip(Button_SlotFeet,    RetrieveGameplayTags::Equipment_Slot_Feet);

	// 무기 슬롯
	if (Button_SlotWeapon)
	{
		const FName WeaponId = InventoryComponent->GetEquippedWeaponId();
		if (WeaponId.IsNone())
		{
			Button_SlotWeapon->SetToolTip(nullptr);
			Button_SlotWeapon->SetToolTipText(FText::GetEmpty());
		}
		else
		{
			FRetrieveItemStack ItemStack;
			ItemStack.ItemId = WeaponId;
			ItemStack.ItemCategoryTag = RetrieveGameplayTags::Item_Weapon;
			ItemStack.Quantity = 1;
			Button_SlotWeapon->SetToolTip(BuildEquipmentSlotTooltipWidget(ItemStack));
		}
	}
}

void UInventoryPanelWidget::HandleSlotHeadClicked()
{
	SelectEquipmentSlot(RetrieveGameplayTags::Equipment_Slot_Head);
}

void UInventoryPanelWidget::HandleSlotWeaponClicked()
{
	if (!InventoryComponent)
	{
		return;
	}
	const FName WeaponId = InventoryComponent->GetEquippedWeaponId();
	if (!WeaponId.IsNone())
	{
		OpenTab(0);
		SelectItem(WeaponId, RetrieveGameplayTags::Item_Weapon,
			InventoryComponent->GetEquippedWeaponSlotInstanceId());
	}
	else
	{
		OpenTab(0);
	}
}

void UInventoryPanelWidget::HandleSlotChestClicked()
{
	SelectEquipmentSlot(RetrieveGameplayTags::Equipment_Slot_Chest);
}

void UInventoryPanelWidget::HandleSlotHandsLClicked()
{
	SelectEquipmentSlot(RetrieveGameplayTags::Equipment_Slot_Hands);
}

void UInventoryPanelWidget::HandleSlotHandsRClicked()
{
	SelectEquipmentSlot(RetrieveGameplayTags::Equipment_Slot_Hands);
}

void UInventoryPanelWidget::HandleSlotLegsClicked()
{
	SelectEquipmentSlot(RetrieveGameplayTags::Equipment_Slot_Legs);
}

void UInventoryPanelWidget::HandleSlotFeetClicked()
{
	SelectEquipmentSlot(RetrieveGameplayTags::Equipment_Slot_Feet);
}

void UInventoryPanelWidget::RefreshSlotIcons()
{
	if (!InventoryComponent)
	{
		return;
	}

	auto SetArmorSlotIcon = [this](UImage* IconImage, FGameplayTag SlotTag)
	{
		if (!IconImage)
		{
			return;
		}
		const FName ArmorId = InventoryComponent->GetEquippedArmorId(SlotTag);
		FRetrieveItemIconRow IconData;
		if (!ArmorId.IsNone() && GetItemIconData(ArmorId, IconData))
		{
			if (UTexture2D* Texture = IconData.IconTexture.LoadSynchronous())
			{
				IconImage->SetBrushFromTexture(Texture);
				IconImage->SetColorAndOpacity(FLinearColor::White);
				return;
			}
		}
		IconImage->SetBrush(FSlateBrush());
		IconImage->SetColorAndOpacity(FLinearColor::Transparent);
	};

	SetArmorSlotIcon(Image_SlotIcon_Head,    RetrieveGameplayTags::Equipment_Slot_Head);
	SetArmorSlotIcon(Image_SlotIcon_Chest,   RetrieveGameplayTags::Equipment_Slot_Chest);
	SetArmorSlotIcon(Image_SlotIcon_Hands_L, RetrieveGameplayTags::Equipment_Slot_Hands);
	SetArmorSlotIcon(Image_SlotIcon_Hands_R, RetrieveGameplayTags::Equipment_Slot_Hands);
	SetArmorSlotIcon(Image_SlotIcon_Legs,    RetrieveGameplayTags::Equipment_Slot_Legs);
	SetArmorSlotIcon(Image_SlotIcon_Feet,    RetrieveGameplayTags::Equipment_Slot_Feet);

	// 무기 슬롯
	if (Image_SlotIcon_Weapon)
	{
		const FName WeaponId = InventoryComponent->GetEquippedWeaponId();
		FRetrieveItemIconRow IconData;
		if (!WeaponId.IsNone() && GetItemIconData(WeaponId, IconData))
		{
			if (UTexture2D* Texture = IconData.IconTexture.LoadSynchronous())
			{
				Image_SlotIcon_Weapon->SetBrushFromTexture(Texture);
				Image_SlotIcon_Weapon->SetColorAndOpacity(FLinearColor::White);
				return;
			}
		}
		Image_SlotIcon_Weapon->SetBrush(FSlateBrush());
		Image_SlotIcon_Weapon->SetColorAndOpacity(FLinearColor::Transparent);
	}
}
