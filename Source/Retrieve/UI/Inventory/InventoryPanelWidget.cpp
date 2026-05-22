#include "UI/Inventory/InventoryPanelWidget.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/InventoryComponent.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/WeaponComponent.h"
#include "Blueprint/WidgetTree.h"
#include "GameFramework/Pawn.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Engine/Texture2D.h"

UInventoryPanelWidget::UInventoryPanelWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	WeaponTabCategoryTag = RetrieveGameplayTags::Item_Weapon;
	ConsumableTabCategoryTag = RetrieveGameplayTags::Item_Consumable;
	CurrentCategoryTag = WeaponTabCategoryTag;
}

void UInventoryPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	InitDefaultTags();
	InitOwnerComponents();
	BindInventoryEvents();
	BindButtonEvents();
	ShowWeaponSwapConfirm(false);
	HideQuickSlotAssignDialog();
	RefreshInventoryView(false);
	RefreshWeaponComparisonText();
	UpdateQuickSlotPanel();
	UpdateQuickSlotActionButtons();
	RefreshInventoryGridLayout();
}

void UInventoryPanelWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshInventoryGridLayout();
}

void UInventoryPanelWidget::InitializeInventoryPanel(UInventoryComponent* InInventoryComponent, UWeaponComponent* InWeaponComponent)
{
	InventoryComponent = InInventoryComponent;
	WeaponComponent = InWeaponComponent;
	InitDefaultTags();
	BindInventoryEvents();
	RefreshInventoryView(false);
	RefreshWeaponComparisonText();
	UpdateQuickSlotPanel();
	UpdateQuickSlotActionButtons();
	OnEquipmentChanged.Broadcast();
}

void UInventoryPanelWidget::OpenTab(int32 TabIndex)
{
	ActiveTabIndex = TabIndex;
	OnTabSwitchRequested.Broadcast(TabIndex);

	if (TabIndex == 0)
	{
		CurrentCategoryTag = WeaponTabCategoryTag;
		RefreshInventoryView(true);
	}
	else if (TabIndex == 1)
	{
		CurrentCategoryTag = ConsumableTabCategoryTag;
		RefreshInventoryView(true);
	}
}

void UInventoryPanelWidget::RefreshInventoryList()
{
	// 목록 갱신 시 기존 선택 해제. 상세 패널은 새 선택 이벤트로 갱신
	RefreshInventoryView(true);
}

void UInventoryPanelWidget::SelectItem(FName ItemId, FGameplayTag ItemCategoryTag)
{
	if (!ItemId.IsNone()
		&& SelectedItemId == ItemId
		&& SelectedItemCategoryTag == ItemCategoryTag)
	{
		ActivateSelectedItem();
		return;
	}

	SelectedItemId = ItemId;
	SelectedItemCategoryTag = ItemCategoryTag;
	ShowWeaponSwapConfirm(false);
	HideQuickSlotAssignDialog();
	RefreshWeaponComparisonText();
	UpdateQuickSlotActionButtons();
	OnSelectedItemChanged.Broadcast(SelectedItemId, SelectedItemCategoryTag);
}

bool UInventoryPanelWidget::ActivateSelectedItem()
{
	if (SelectedItemId.IsNone())
	{
		return false;
	}

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

	return false;
}

bool UInventoryPanelWidget::EquipSelectedWeapon()
{
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

	const bool bEquipped = InventoryComponent->RequestEquipWeapon(SelectedItemId);
	if (bEquipped)
	{
		RefreshWeaponComparisonText();
		OnEquipmentChanged.Broadcast();
		OnSelectedItemChanged.Broadcast(SelectedItemId, SelectedItemCategoryTag);
		OnInventoryListChanged.Broadcast();
	}
	return bEquipped;
}

bool UInventoryPanelWidget::UnequipCurrentWeapon()
{
	if (!CanUnequipCurrentWeapon())
	{
		return false;
	}

	const bool bUnequipped = InventoryComponent->RequestUnequipWeapon();
	if (bUnequipped)
	{
		ShowWeaponSwapConfirm(false);
		RefreshWeaponComparisonText();
		OnEquipmentChanged.Broadcast();
		OnSelectedItemChanged.Broadcast(SelectedItemId, SelectedItemCategoryTag);
		OnInventoryListChanged.Broadcast();
	}
	return bUnequipped;
}

bool UInventoryPanelWidget::UseSelectedConsumable()
{
	if (!CanUseSelectedConsumable())
	{
		return false;
	}

	const FName UsedItemId = SelectedItemId;
	const FGameplayTag UsedCategoryTag = SelectedItemCategoryTag;
	const bool bUsed = InventoryComponent->UseConsumableItem(UsedItemId);
	if (bUsed)
	{
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
	if (!InventoryComponent || !CanAssignSelectedConsumableToQuickSlot())
	{
		return false;
	}

	const bool bAssigned = InventoryComponent->AssignConsumableSlot(SlotKey, SelectedItemId);
	if (bAssigned)
	{
		HideQuickSlotAssignDialog();
		UpdateQuickSlotPanel();
		UpdateQuickSlotActionButtons();
		OnInventoryListChanged.Broadcast();
	}
	return bAssigned;
}

bool UInventoryPanelWidget::UnassignSelectedConsumableSlot()
{
	if (!InventoryComponent || !IsSelectedConsumableAssignedToQuickSlot())
	{
		return false;
	}

	const int32 SlotKey = GetSelectedConsumableSlotKey();
	const bool bUnassigned = InventoryComponent->UnassignConsumableSlot(SlotKey);
	if (bUnassigned)
	{
		HideQuickSlotAssignDialog();
		UpdateQuickSlotPanel();
		UpdateQuickSlotActionButtons();
		OnInventoryListChanged.Broadcast();
	}
	return bUnassigned;
}

TArray<FRetrieveItemStack> UInventoryPanelWidget::GetCurrentItems() const
{
	return InventoryComponent
		? InventoryComponent->GetItemsByCategory(CurrentCategoryTag)
		: TArray<FRetrieveItemStack>();
}

bool UInventoryPanelWidget::IsSelectedWeaponEquipped() const
{
	return IsWeaponItemEquipped(SelectedItemId) && IsWeaponCategory(SelectedItemCategoryTag);
}

bool UInventoryPanelWidget::IsItemSelected(FName ItemId) const
{
	return !ItemId.IsNone() && SelectedItemId == ItemId;
}

bool UInventoryPanelWidget::IsWeaponItemEquipped(FName WeaponItemId) const
{
	return InventoryComponent
		&& !WeaponItemId.IsNone()
		&& InventoryComponent->GetEquippedWeaponId() == WeaponItemId;
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

bool UInventoryPanelWidget::CanAssignSelectedConsumableToQuickSlot() const
{
	return CanUseSelectedConsumable();
}

bool UInventoryPanelWidget::IsSelectedConsumableAssignedToQuickSlot() const
{
	return GetSelectedConsumableSlotKey() != INDEX_NONE;
}

int32 UInventoryPanelWidget::GetSelectedConsumableSlotKey() const
{
	return InventoryComponent && IsConsumableCategory(SelectedItemCategoryTag)
		? InventoryComponent->GetAssignedConsumableSlotKey(SelectedItemId)
		: INDEX_NONE;
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

void UInventoryPanelWidget::RefreshWeaponComparisonText()
{
	if (Text_SelectedCompare)
	{
		Text_SelectedCompare->SetText(FText::FromString(BuildWeaponComparisonText()));
	}
	RefreshWeaponSkillIcons();
}

void UInventoryPanelWidget::HandleInventoryChanged()
{
	RefreshWeaponComparisonText();
	UpdateQuickSlotPanel();
	UpdateQuickSlotActionButtons();
	OnInventoryListChanged.Broadcast();
}

void UInventoryPanelWidget::HandleEquippedWeaponChanged(FName WeaponItemId)
{
	RefreshWeaponComparisonText();
	OnEquipmentChanged.Broadcast();
	OnInventoryListChanged.Broadcast();
	if (IsWeaponCategory(SelectedItemCategoryTag))
	{
		OnSelectedItemChanged.Broadcast(SelectedItemId, SelectedItemCategoryTag);
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
	ShowQuickSlotAssignDialog();
}

void UInventoryPanelWidget::HandleUnassignQuickSlotClicked()
{
	UnassignSelectedConsumableSlot();
}

void UInventoryPanelWidget::HandleAssignSlot4Clicked()
{
	AssignSelectedConsumableToSlot(4);
}

void UInventoryPanelWidget::HandleAssignSlot5Clicked()
{
	AssignSelectedConsumableToSlot(5);
}

void UInventoryPanelWidget::HandleCancelQuickSlotAssignClicked()
{
	HideQuickSlotAssignDialog();
}

void UInventoryPanelWidget::HandleConsumableSlotChanged(int32 SlotKey, FName ItemId)
{
	UpdateQuickSlotPanel();
	UpdateQuickSlotActionButtons();
	OnInventoryListChanged.Broadcast();
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
	}
}

void UInventoryPanelWidget::BindButtonEvents()
{
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
	if (!CurrentCategoryTag.IsValid())
	{
		CurrentCategoryTag = ActiveTabIndex == 1 ? ConsumableTabCategoryTag : WeaponTabCategoryTag;
	}
}

void UInventoryPanelWidget::RefreshInventoryView(bool bClearSelection)
{
	InitDefaultTags();

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
	ShowWeaponSwapConfirm(false);
	HideQuickSlotAssignDialog();
	RefreshWeaponComparisonText();
	UpdateQuickSlotActionButtons();
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

bool UInventoryPanelWidget::ShouldConfirmWeaponSwap() const
{
	if (!InventoryComponent || SelectedItemId.IsNone() || !IsWeaponCategory(SelectedItemCategoryTag))
	{
		return false;
	}

	const FName EquippedWeaponId = InventoryComponent->GetEquippedWeaponId();
	return !EquippedWeaponId.IsNone() && EquippedWeaponId != SelectedItemId;
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
		Text_QuickSlot4->SetText(GetQuickSlotDisplayText(4));
	}
	if (Text_QuickSlot5)
	{
		Text_QuickSlot5->SetText(GetQuickSlotDisplayText(5));
	}
}

void UInventoryPanelWidget::UpdateQuickSlotActionButtons()
{
	const bool bCanAssign = CanAssignSelectedConsumableToQuickSlot();
	const bool bIsAssigned = IsSelectedConsumableAssignedToQuickSlot();

	if (Button_AssignQuickSlot)
	{
		Button_AssignQuickSlot->SetVisibility(bCanAssign && !bIsAssigned ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (Button_UnassignQuickSlot)
	{
		Button_UnassignQuickSlot->SetVisibility(bCanAssign && bIsAssigned ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (!bCanAssign)
	{
		HideQuickSlotAssignDialog();
	}
}

void UInventoryPanelWidget::RefreshInventoryGridLayout()
{
	if (!UniformGrid_ItemList)
	{
		return;
	}

	constexpr int32 GridColumnCount = 4;
	constexpr float MinSlotSize = 48.0f;
	constexpr float ScrollbarAllowance = 14.0f;

	const FVector2D GridAreaSize = ScrollBox_ItemList
		? ScrollBox_ItemList->GetCachedGeometry().GetLocalSize()
		: UniformGrid_ItemList->GetCachedGeometry().GetLocalSize();
	if (GridAreaSize.X <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float AvailableWidth = FMath::Max(GridAreaSize.X - ScrollbarAllowance, MinSlotSize * GridColumnCount);
	const float SlotSize = AvailableWidth / static_cast<float>(GridColumnCount);
	const bool bGridAreaChanged = !GridAreaSize.Equals(LastInventoryGridAreaSize, 0.5f);
	if (bGridAreaChanged)
	{
		UniformGrid_ItemList->SetMinDesiredSlotWidth(SlotSize);
		UniformGrid_ItemList->SetMinDesiredSlotHeight(SlotSize);
		LastInventoryGridAreaSize = GridAreaSize;
	}

	const int32 ChildCount = UniformGrid_ItemList->GetChildrenCount();
	for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
	{
		UWidget* Child = UniformGrid_ItemList->GetChildAt(ChildIndex);
		if (UUniformGridSlot* GridSlot = Cast<UUniformGridSlot>(Child ? Child->Slot : nullptr))
		{
			GridSlot->SetHorizontalAlignment(HAlign_Fill);
			GridSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}
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
				SkillIcon->SetDesiredSizeOverride(FVector2D(24.0f, 24.0f));
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
