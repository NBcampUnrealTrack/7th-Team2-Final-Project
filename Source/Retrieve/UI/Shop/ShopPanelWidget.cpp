#include "UI/Shop/ShopPanelWidget.h"
#include "Components/Inventory/InventoryComponent.h"
#include "Components/World/RetrieveShopComponent.h"
#include "Shop/RetrieveShopDefinitionAsset.h"
#include "Shop/RetrieveShopTypes.h"
#include "Data/RetrieveDataTableTypes.h"
#include "UI/Shop/ShopItemEntryWidget.h"
#include "UI/Shop/ShopSellSlotWidget.h"
#include "UI/Shop/ShopRepurchaseEntryWidget.h"
#include "Save/RetrieveSaveSubsystem.h"
#include "Save/RetrieveSaveGame.h"
#include "Player/RetrievePlayerController.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/SizeBox.h"
#include "Components/OverlaySlot.h"
#include "Components/WidgetSwitcherSlot.h"
#include "Components/Widget.h"
#include "Components/WidgetSwitcher.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Texture2D.h"
#include "Styling/SlateBrush.h"

static const FRetrieveShopItemRow* FindShopRow(const URetrieveShopDefinitionAsset* Def,
                                               const URetrieveShopComponent* Comp,
                                               FName RowName, const TCHAR* Context)
{
	if (!Def || RowName.IsNone()) return nullptr;
	if (Def->ShopItemTable)
	{
		if (const FRetrieveShopItemRow* Row = Def->ShopItemTable->FindRow<FRetrieveShopItemRow>(RowName, Context))
			return Row;
	}
	if (Def->RotatingPoolTable)
	{
		if (const FRetrieveShopItemRow* Row = Def->RotatingPoolTable->FindRow<FRetrieveShopItemRow>(RowName, Context))
			return Row;
	}
	return nullptr;
}

void UShopPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindButtonEvents();
}

void UShopPanelWidget::InitializeShopPanel(UInventoryComponent* InInventoryComp,
                                           URetrieveShopDefinitionAsset* InShopDefinition,
                                           URetrieveShopComponent* InShopComponent)
{
	InventoryComponent = InInventoryComp;
	ShopDefinition     = InShopDefinition;
	ShopComponentRef   = InShopComponent;

	if (InventoryComponent)
	{
		InventoryComponent->OnCurrencyChanged.AddDynamic(this, &UShopPanelWidget::HandleCurrencyChanged);
		InventoryComponent->OnInventoryChanged.AddDynamic(this, &UShopPanelWidget::HandleInventoryChanged);
	}

	if (ShopDefinition)
	{
		SetNpcNameText(ShopDefinition->ShopName);
		ShowShopDialogue(DialogueRow_Greeting, ShopDefinition->GreetingText);
	}

	ApplyShopTypeStyle();

	// 3열 레이아웃: 중앙 상세 패널 항상 표시 (품목 선택 전엔 빈 상태)
	if (Panel_BuyDetail) Panel_BuyDetail->SetVisibility(ESlateVisibility::Visible);
	if (Text_ItemName)   Text_ItemName->SetText(FText::FromString(TEXT("← 상품을 선택하세요")));

	LoadRepurchaseHistoryFromSave();
	RefreshCurrencyText();

	// 구매 목록(좌)과 인벤토리 그리드(우) 동시 초기화
	RefreshBuyList();
	static const FGameplayTag WeaponTag = FGameplayTag::RequestGameplayTag("Item.Weapon");
	if (Switcher_SellView) Switcher_SellView->SetActiveWidgetIndex(0);
	RefreshSellGrid(WeaponTag);
	SwitchToBuyTab();
}

// ────────────────────────────────────────────────────────────────────────────
// 탭 전환
// ────────────────────────────────────────────────────────────────────────────

void UShopPanelWidget::SwitchToBuyTab()
{
	bSellModeActive = false;
	if (Switcher_Tabs)
	{
		Switcher_Tabs->SetActiveWidgetIndex(0);
	}
	ApplyShopModeLayout(false);
	RefreshBuyList();
	RefreshBuyDetail();
}

void UShopPanelWidget::SwitchToSellTab()
{
	bSellModeActive = true;
	if (Switcher_Tabs)
	{
		Switcher_Tabs->SetActiveWidgetIndex(1);
	}
	RestoreSellGridView();
	ApplyShopModeLayout(true);
	if (!CurrentSellCategoryTag.IsValid())
	{
		CurrentSellCategoryTag = FGameplayTag::RequestGameplayTag("Item.Weapon");
	}
	RefreshSellGrid(CurrentSellCategoryTag);
}

// ────────────────────────────────────────────────────────────────────────────
// 구매 탭
// ────────────────────────────────────────────────────────────────────────────

void UShopPanelWidget::SelectBuyItem(FName ShopRowName)
{
	SelectedBuyRowName = ShopRowName;
	BuyQuantity = 1;
	RefreshBuyDetail();
}

bool UShopPanelWidget::ExecuteBuy()
{
	if (!InventoryComponent || SelectedBuyRowName.IsNone())
	{
		ShowShopDialogue(DialogueRow_BuyFail_OutOfStock, INVTEXT("지금은 판매할 수 없는 물건이야."));
		return false;
	}

	const int32 TotalCost = SelectedBuyPrice * BuyQuantity;
	if (!InventoryComponent->HasEnoughCurrency(TotalCost))
	{
		ShowShopDialogue(DialogueRow_BuyFail_NotEnoughMoney, INVTEXT("돈이 부족한 것 같아."));
		return false;
	}

	const FRetrieveShopItemRow* Row = ShopDefinition
		? FindShopRow(ShopDefinition, ShopComponentRef, SelectedBuyRowName, TEXT("ExecuteBuy"))
		: nullptr;

	if (!Row)
	{
		ShowShopDialogue(DialogueRow_BuyFail_OutOfStock, INVTEXT("미안해, 그 물건은 지금 재고가 없어."));
		return false;
	}

	// 무료 상품은 재화를 차감할 필요가 없다. SpendCurrency(0)는 유효한 지출로
	// 취급하지 않으므로, 양수 가격일 때만 실제 차감을 시도한다.
	if (TotalCost > 0 && !InventoryComponent->SpendCurrency(TotalCost))
	{
		ShowShopDialogue(DialogueRow_BuyFail_NotEnoughMoney, INVTEXT("돈이 부족한 것 같아."));
		return false;
	}

	InventoryComponent->AddItem(Row->ItemId, Row->ItemCategoryTag, BuyQuantity);
	OnItemPurchased.Broadcast(Row->ItemId, BuyQuantity);

	ShowShopDialogue(DialogueRow_BuySuccess, INVTEXT("좋은 선택이야. 잘 써."));
	RefreshCurrencyText();
	RefreshBuyDetail();
	return true;
}

bool UShopPanelWidget::CanExecuteBuy() const
{
	if (!InventoryComponent || SelectedBuyRowName.IsNone()) return false;
	return InventoryComponent->HasEnoughCurrency(SelectedBuyPrice * BuyQuantity);
}

// ────────────────────────────────────────────────────────────────────────────
// 판매 탭 — 그리드 갱신
// ────────────────────────────────────────────────────────────────────────────

void UShopPanelWidget::RefreshSellGrid(FGameplayTag CategoryTag)
{
	CurrentSellCategoryTag = CategoryTag;
	CurrentSellSlots.Empty();
	SellSlotWidgets.Empty();
	SelectedSlotIndices.Empty();
	LastClickedSlotIndex = -1;
	DragStartSlotIndex   = -1;
	bIsDragSelecting     = false;
	bDragIsShiftDrag     = false;

	if (!UniformGrid_SellItems)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[ShopPanel] UniformGrid_SellItems is missing. Place a UniformGridPanel named 'UniformGrid_SellItems' in WBP_ShopPanel."));
		return;
	}

	UniformGrid_SellItems->ClearChildren();

	if (!InventoryComponent) return;

	TArray<FRetrieveItemStack> Stacks = InventoryComponent->GetItemsByCategory(CategoryTag);
	for (const FRetrieveItemStack& Stack : Stacks)
	{
		if (Stack.Quantity <= 0) continue;

		FSellSlotCache Cache;
		Cache.ItemId      = Stack.ItemId;
		Cache.CategoryTag = Stack.ItemCategoryTag;
		Cache.Quantity    = Stack.Quantity;
		const int32 SlotIdx = CurrentSellSlots.Num();
		CurrentSellSlots.Add(Cache);

		if (SellSlotWidgetClass && UniformGrid_SellItems)
		{
			UShopSellSlotWidget* SlotWidget = CreateWidget<UShopSellSlotWidget>(this, SellSlotWidgetClass);
			if (SlotWidget)
			{
				SlotWidget->ApplyMinimumSize(SellSlotMinSize);
				SlotWidget->InitSlot(SlotIdx, this);
				SlotWidget->SetSlotData(
					Stack.ItemId,
					Stack.ItemCategoryTag,
					Stack.Quantity,
					GetItemIcon(Stack.ItemId),
					GetItemDisplayName(Stack.ItemId, Stack.ItemCategoryTag),
					GetItemShortDesc(Stack.ItemId, Stack.ItemCategoryTag),
					ItemDetailTooltipClass);
				const int32 Col = SlotIdx % SellGridColumns;
				const int32 Row = SlotIdx / SellGridColumns;
				if (UUniformGridSlot* UGS = Cast<UUniformGridSlot>(
					UniformGrid_SellItems->AddChildToUniformGrid(SlotWidget, Row, Col)))
				{
					UGS->SetHorizontalAlignment(HAlign_Fill);
					UGS->SetVerticalAlignment(VAlign_Fill);
				}
				SellSlotWidgets.Add(SlotWidget);
			}
		}
	}

	// 최소 행 수만큼 빈 슬롯으로 채워서 항상 4열 그리드가 보이도록
	const int32 TotalMinSlots = SellGridMinRows * SellGridColumns;
	const int32 FilledSlots   = CurrentSellSlots.Num();
	if (SellSlotWidgetClass && UniformGrid_SellItems && FilledSlots < TotalMinSlots)
	{
		for (int32 i = FilledSlots; i < TotalMinSlots; ++i)
		{
			UShopSellSlotWidget* EmptySlot = CreateWidget<UShopSellSlotWidget>(this, SellSlotWidgetClass);
			if (EmptySlot)
			{
				EmptySlot->ApplyMinimumSize(SellSlotMinSize);
				EmptySlot->ClearSlot();
				EmptySlot->SetIsEnabled(false);
				if (UUniformGridSlot* UGS = Cast<UUniformGridSlot>(
					UniformGrid_SellItems->AddChildToUniformGrid(EmptySlot, i / SellGridColumns, i % SellGridColumns)))
				{
					UGS->SetHorizontalAlignment(HAlign_Fill);
					UGS->SetVerticalAlignment(VAlign_Fill);
				}
			}
		}
	}

	UpdateSellSlotVisuals();
	UpdateSellToolbar();
}

// ────────────────────────────────────────────────────────────────────────────
// 판매 탭 — 다중선택
// ────────────────────────────────────────────────────────────────────────────

void UShopPanelWidget::HandleSellSlotPressed(int32 SlotIndex, bool bShiftHeld)
{
	bIsDragSelecting  = true;
	bDragIsShiftDrag  = bShiftHeld;

	if (bShiftHeld && LastClickedSlotIndex >= 0)
	{
		// Shift+클릭: 앵커에서 현재 슬롯까지 범위 선택
		// 드래그 시작점은 앵커로 유지
		DragStartSlotIndex = LastClickedSlotIndex;
		SelectRangeInSellGrid(LastClickedSlotIndex, SlotIndex);
	}
	else
	{
		// 일반 클릭 또는 최초 Shift 클릭: 이 슬롯만 선택, 앵커 초기화
		DragStartSlotIndex   = SlotIndex;
		LastClickedSlotIndex = SlotIndex;
		SelectedSlotIndices.Empty();
		SelectedSlotIndices.Add(SlotIndex);
	}

	UpdateSellSlotVisuals();
	UpdateSellToolbar();
}

void UShopPanelWidget::HandleSellSlotEntered(int32 SlotIndex)
{
	// Shift+drag 중일 때만 범위 확장
	if (!bIsDragSelecting || !bDragIsShiftDrag) return;
	SelectRangeInSellGrid(DragStartSlotIndex, SlotIndex);
	UpdateSellSlotVisuals();
	UpdateSellToolbar();
}

void UShopPanelWidget::EndDragSelect()
{
	bIsDragSelecting = false;
	bDragIsShiftDrag = false;
}

void UShopPanelWidget::SelectRangeInSellGrid(int32 A, int32 B)
{
	const int32 Start = FMath::Min(A, B);
	const int32 End   = FMath::Max(A, B);
	SelectedSlotIndices.Empty();
	for (int32 i = Start; i <= End && i < CurrentSellSlots.Num(); ++i)
		SelectedSlotIndices.Add(i);
}

void UShopPanelWidget::UpdateSellSlotVisuals()
{
	for (int32 i = 0; i < SellSlotWidgets.Num(); ++i)
	{
		if (SellSlotWidgets[i])
			SellSlotWidgets[i]->SetSelected(SelectedSlotIndices.Contains(i));
	}
}

void UShopPanelWidget::UpdateSellToolbar()
{
	const int32 SelectedCount = SelectedSlotIndices.Num();
	int32 TotalPrice = 0;

	for (int32 Idx : SelectedSlotIndices)
	{
		if (CurrentSellSlots.IsValidIndex(Idx))
		{
			const FSellSlotCache& S = CurrentSellSlots[Idx];
			TotalPrice += CalcSellPrice(S.ItemId, S.CategoryTag) * S.Quantity;
		}
	}

	if (Text_SelectedCount)
	{
		if (SelectedCount > 0)
		{
			Text_SelectedCount->SetText(
				FText::Format(INVTEXT("{0}개 선택"), FText::AsNumber(SelectedCount)));
		}
		else
		{
			Text_SelectedCount->SetText(INVTEXT("선택 없음"));
		}
	}

	if (Text_SelectedCurrency)
	{
		if (SelectedCount > 0)
		{
			Text_SelectedCurrency->SetText(
				FText::Format(INVTEXT("{0} G"), FText::AsNumber(TotalPrice)));
		}
		else
		{
			Text_SelectedCurrency->SetText(INVTEXT("0 G"));
		}
	}

	if (Button_SellSelected)
	{
		Button_SellSelected->SetIsEnabled(SelectedCount > 0 && InventoryComponent != nullptr);
	}

	if (Button_OpenRepurchase)
	{
		Button_OpenRepurchase->SetIsEnabled(RepurchaseHistory.Num() > 0);
	}

	if (CheckBox_SelectAll && CurrentSellSlots.Num() > 0)
	{
		const bool bAll = SelectedSlotIndices.Num() == CurrentSellSlots.Num();
		CheckBox_SelectAll->SetCheckedState(bAll ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
	}
}

// ────────────────────────────────────────────────────────────────────────────
// 판매 실행
// ────────────────────────────────────────────────────────────────────────────

bool UShopPanelWidget::ExecuteSellSelected()
{
	if (!CanExecuteSellSelected())
	{
		ShowShopDialogue(DialogueRow_SellFail_NoSelection, INVTEXT("팔 물건을 먼저 골라줘."));
		return false;
	}

	// 현재 상점은 별도 보유금 시스템이 없어서 기본적으로 상인이 항상 구매 가능한 것으로 처리한다.
	// 추후 상점 보유금을 추가하면 이 지점에서 DialogueRow_SellFail_MerchantNoMoney를 출력하면 된다.

	// 인덱스를 내림차순으로 정렬하여 제거 시 인덱스 어긋남 방지
	TArray<int32> SortedIndices = SelectedSlotIndices.Array();
	SortedIndices.Sort([](int32 A, int32 B) { return A > B; });

	bool bAnySold = false;
	for (int32 Idx : SortedIndices)
	{
		if (!CurrentSellSlots.IsValidIndex(Idx)) continue;
		const FSellSlotCache& S = CurrentSellSlots[Idx];

		const int32 Price = CalcSellPrice(S.ItemId, S.CategoryTag) * S.Quantity;
		if (InventoryComponent->RemoveItem(S.ItemId, S.CategoryTag, S.Quantity))
		{
			InventoryComponent->AddCurrency(Price);
			PushRepurchaseRecord(S.ItemId, S.CategoryTag, S.Quantity, Price);
			OnItemSold.Broadcast(S.ItemId, S.Quantity);
			bAnySold = true;
		}
	}

	if (bAnySold)
	{
		ShowShopDialogue(DialogueRow_SellSuccess, INVTEXT("좋은 거래였어."));
		SyncRepurchaseHistoryToSave();
		RefreshCurrencyText();
		RefreshSellGrid(CurrentSellCategoryTag);
	}
	else
	{
		ShowShopDialogue(DialogueRow_SellFail_MerchantNoMoney, INVTEXT("미안해, 지금은 내가 살 수 없겠어."));
	}
	return bAnySold;
}

bool UShopPanelWidget::CanExecuteSellSelected() const
{
	return InventoryComponent && !SelectedSlotIndices.IsEmpty();
}

// ────────────────────────────────────────────────────────────────────────────
// 재구매
// ────────────────────────────────────────────────────────────────────────────

void UShopPanelWidget::ExecuteRepurchase(int32 HistoryIndex)
{
	if (!InventoryComponent || !RepurchaseHistory.IsValidIndex(HistoryIndex)) return;

	FShopRepurchaseRecord& Record = RepurchaseHistory[HistoryIndex];
	const int32 SanitizedPrice = GetSanitizedRepurchasePrice(
		Record.ItemId, Record.ItemCategoryTag, Record.Quantity, Record.RepurchasePrice);
	if (SanitizedPrice <= 0) return;

	Record.RepurchasePrice = SanitizedPrice;
	if (!InventoryComponent->HasEnoughCurrency(Record.RepurchasePrice)) return;

	if (InventoryComponent->SpendCurrency(Record.RepurchasePrice))
	{
		InventoryComponent->AddItem(Record.ItemId, Record.ItemCategoryTag, Record.Quantity);
		RepurchaseHistory.RemoveAt(HistoryIndex);
		SyncRepurchaseHistoryToSave();
		RefreshCurrencyText();
		RefreshRepurchaseList();

		if (RepurchaseHistory.IsEmpty())
		{
			// 히스토리 소진 → 판매 그리드로 복귀
			if (Switcher_SellView) Switcher_SellView->SetActiveWidgetIndex(0);
		}
	}
}

void UShopPanelWidget::ExecuteRepurchaseAll()
{
	if (!InventoryComponent) return;

	// 재구매 가능한 항목만 처리 (화폐 부족 항목은 건너뜀)
	for (int32 i = RepurchaseHistory.Num() - 1; i >= 0; --i)
	{
		FShopRepurchaseRecord& Record = RepurchaseHistory[i];
		const int32 SanitizedPrice = GetSanitizedRepurchasePrice(
			Record.ItemId, Record.ItemCategoryTag, Record.Quantity, Record.RepurchasePrice);
		if (SanitizedPrice <= 0)
		{
			RepurchaseHistory.RemoveAt(i);
			continue;
		}

		Record.RepurchasePrice = SanitizedPrice;
		if (InventoryComponent->HasEnoughCurrency(SanitizedPrice)
			&& InventoryComponent->SpendCurrency(SanitizedPrice))
		{
			InventoryComponent->AddItem(Record.ItemId, Record.ItemCategoryTag, Record.Quantity);
			RepurchaseHistory.RemoveAt(i);
		}
	}

	SyncRepurchaseHistoryToSave();
	RefreshCurrencyText();

	if (RepurchaseHistory.IsEmpty())
	{
		if (Switcher_SellView) Switcher_SellView->SetActiveWidgetIndex(0);
	}
	else
	{
		RefreshRepurchaseList();
	}
}

void UShopPanelWidget::RefreshRepurchaseList()
{
	if (!ScrollBox_RepurchaseList) return;
	ScrollBox_RepurchaseList->ClearChildren();

	int32 TotalCost = 0;
	bool bHistoryChanged = false;
	for (int32 i = 0; i < RepurchaseHistory.Num(); ++i)
	{
		FShopRepurchaseRecord& Record = RepurchaseHistory[i];
		const int32 SanitizedPrice = GetSanitizedRepurchasePrice(
			Record.ItemId, Record.ItemCategoryTag, Record.Quantity, Record.RepurchasePrice);
		if (SanitizedPrice <= 0)
		{
			bHistoryChanged = true;
			RepurchaseHistory.RemoveAt(i--);
			continue;
		}
		if (SanitizedPrice != Record.RepurchasePrice)
		{
			Record.RepurchasePrice = SanitizedPrice;
			bHistoryChanged = true;
		}
		TotalCost += Record.RepurchasePrice;

		if (!RepurchaseEntryWidgetClass) continue;

		UShopRepurchaseEntryWidget* Entry = CreateWidget<UShopRepurchaseEntryWidget>(
			this, RepurchaseEntryWidgetClass);
		if (!Entry) continue;

		FText NameTxt  = GetItemDisplayName(Record.ItemId, Record.ItemCategoryTag);
		FText PriceTxt = FText::Format(INVTEXT("{0}골드"), FText::AsNumber(Record.RepurchasePrice));
		const bool bCanRepurchase = InventoryComponent && InventoryComponent->HasEnoughCurrency(Record.RepurchasePrice);
		Entry->InitEntry(i, NameTxt, PriceTxt, this, bCanRepurchase);
		ScrollBox_RepurchaseList->AddChild(Entry);
	}

	if (bHistoryChanged)
	{
		SyncRepurchaseHistoryToSave();
	}

	if (Text_RepurchaseTotalCost)
	{
		Text_RepurchaseTotalCost->SetText(
			FText::Format(INVTEXT("합계 {0}골드"), FText::AsNumber(TotalCost)));
	}
	if (Button_RepurchaseAll)
		Button_RepurchaseAll->SetIsEnabled(!RepurchaseHistory.IsEmpty() && TotalCost > 0);
}

// ────────────────────────────────────────────────────────────────────────────
// 재구매 히스토리 — 세이브 연동
// ────────────────────────────────────────────────────────────────────────────

void UShopPanelWidget::PushRepurchaseRecord(FName ItemId, FGameplayTag Category, int32 Qty, int32 Price)
{
	const int32 SanitizedPrice = GetSanitizedRepurchasePrice(ItemId, Category, Qty, Price);
	if (SanitizedPrice <= 0)
	{
		return;
	}

	FShopRepurchaseRecord Record;
	Record.ItemId          = ItemId;
	Record.ItemCategoryTag = Category;
	Record.Quantity        = Qty;
	Record.RepurchasePrice = SanitizedPrice;
	RepurchaseHistory.Insert(Record, 0); // 최신 항목이 맨 앞

	// 최대 보관 건수 초과 시 오래된 항목 제거
	while (RepurchaseHistory.Num() > MaxRepurchaseHistory)
		RepurchaseHistory.RemoveAt(RepurchaseHistory.Num() - 1);
}

void UShopPanelWidget::SyncRepurchaseHistoryToSave()
{
	if (UGameInstance* GI = UGameplayStatics::GetGameInstance(GetWorld()))
	{
		if (URetrieveSaveSubsystem* Sub = GI->GetSubsystem<URetrieveSaveSubsystem>())
		{
			if (URetrieveSaveGame* Save = Sub->GetCurrentSaveGame())
			{
				Save->ShopRepurchaseHistory = RepurchaseHistory;
				Sub->FlushWorldState();
			}
		}
	}
}

void UShopPanelWidget::LoadRepurchaseHistoryFromSave()
{
	RepurchaseHistory.Empty();
	if (UGameInstance* GI = UGameplayStatics::GetGameInstance(GetWorld()))
	{
		if (URetrieveSaveSubsystem* Sub = GI->GetSubsystem<URetrieveSaveSubsystem>())
		{
			if (URetrieveSaveGame* Save = Sub->GetCurrentSaveGame())
			{
				RepurchaseHistory = Save->ShopRepurchaseHistory;
			}
		}
	}

	bool bHistoryChanged = false;
	for (int32 i = RepurchaseHistory.Num() - 1; i >= 0; --i)
	{
		FShopRepurchaseRecord& Record = RepurchaseHistory[i];
		const int32 SanitizedPrice = GetSanitizedRepurchasePrice(
			Record.ItemId, Record.ItemCategoryTag, Record.Quantity, Record.RepurchasePrice);
		if (SanitizedPrice <= 0)
		{
			RepurchaseHistory.RemoveAt(i);
			bHistoryChanged = true;
			continue;
		}
		if (SanitizedPrice != Record.RepurchasePrice)
		{
			Record.RepurchasePrice = SanitizedPrice;
			bHistoryChanged = true;
		}
	}

	if (bHistoryChanged)
	{
		SyncRepurchaseHistoryToSave();
	}
}

int32 UShopPanelWidget::GetSanitizedRepurchasePrice(FName ItemId, FGameplayTag CategoryTag, int32 Quantity, int32 StoredPrice) const
{
	if (ItemId.IsNone() || Quantity <= 0)
	{
		return 0;
	}

	const int32 UnitSellPrice = CalcSellPrice(ItemId, CategoryTag);
	if (UnitSellPrice <= 0)
	{
		return 0;
	}

	const int64 ExpectedTotal64 = static_cast<int64>(UnitSellPrice) * static_cast<int64>(Quantity);
	if (ExpectedTotal64 <= 0 || ExpectedTotal64 > TNumericLimits<int32>::Max())
	{
		return 0;
	}

	const int32 ExpectedTotal = static_cast<int32>(ExpectedTotal64);
	if (StoredPrice <= 0 || static_cast<int64>(StoredPrice) > ExpectedTotal64 * 2)
	{
		return ExpectedTotal;
	}

	return StoredPrice;
}

// ────────────────────────────────────────────────────────────────────────────
// NPC 이름 / 대사
// ────────────────────────────────────────────────────────────────────────────

void UShopPanelWidget::SetNpcNameText(const FText& InText)
{
	if (Text_NpcName)
	{
		Text_NpcName->SetText(InText);
		return;
	}

	// WBP 변수명이 TXT_Name인 경우도 지원
	if (UTextBlock* NameText = Cast<UTextBlock>(GetWidgetFromName(TEXT("TXT_Name"))))
	{
		NameText->SetText(InText);
	}
}

void UShopPanelWidget::SetNpcDialogueText(const FText& InText)
{
	if (Text_NpcDialogue)
	{
		Text_NpcDialogue->SetText(InText);
		return;
	}

	// WBP 변수명이 TXT_Dialogue인 경우도 지원
	if (UTextBlock* DialogueText = Cast<UTextBlock>(GetWidgetFromName(TEXT("TXT_Dialogue"))))
	{
		DialogueText->SetText(InText);
	}
}

FText UShopPanelWidget::GetShopDialogueText(FName DialogueRowName, const FText& FallbackText) const
{
	if (!ShopDialogueTable || DialogueRowName.IsNone())
	{
		return FallbackText;
	}

	const FRetrieveShopDialogueRow* Row = ShopDialogueTable->FindRow<FRetrieveShopDialogueRow>(
		DialogueRowName, TEXT("ShopDialogue"));
	if (!Row)
	{
		return FallbackText;
	}

	return Row->DialogueText.IsEmpty() ? FallbackText : Row->DialogueText;
}

void UShopPanelWidget::ShowShopDialogue(FName DialogueRowName, const FText& FallbackText)
{
	SetNpcDialogueText(GetShopDialogueText(DialogueRowName, FallbackText));
}

// ────────────────────────────────────────────────────────────────────────────
// 공통 유틸
// ────────────────────────────────────────────────────────────────────────────

void UShopPanelWidget::ApplyShopTypeStyle()
{
	if (!ShopDefinition) return;

	FText BadgeLabel;
	switch (ShopDefinition->ShopType)
	{
	case ERetrieveShopType::Weapon:
		BadgeLabel = INVTEXT("무기");   break;
	case ERetrieveShopType::Armor:
		BadgeLabel = INVTEXT("방어구"); break;
	default:
		BadgeLabel = INVTEXT("잡화");   break;
	}

	if (Text_ShopTypeBadge) Text_ShopTypeBadge->SetText(BadgeLabel);
	if (Text_TabBuyLabel)   Text_TabBuyLabel->SetText(INVTEXT("구매"));
	if (Text_TabSellLabel)  Text_TabSellLabel->SetText(INVTEXT("판매"));
}

void UShopPanelWidget::ApplyShopModeLayout(bool bSellMode)
{
	UWidget* CardList = GetWidgetFromName(TEXT("Card_List"));
	UWidget* CardDetail = GetWidgetFromName(TEXT("Card_Detail"));
	UWidget* CardInventory = GetWidgetFromName(TEXT("Card_Inven"));

	const ESlateVisibility ActiveVisibility = ESlateVisibility::Visible;
	const ESlateVisibility HiddenVisibility = ESlateVisibility::Collapsed;

	if (CardList)
	{
		CardList->SetVisibility(bSellMode ? HiddenVisibility : ActiveVisibility);
	}
	if (CardDetail)
	{
		CardDetail->SetVisibility(bSellMode ? HiddenVisibility : ActiveVisibility);
	}
	if (Panel_BuyDetail)
	{
		Panel_BuyDetail->SetVisibility(bSellMode ? HiddenVisibility : ActiveVisibility);
	}
	if (CardInventory)
	{
		CardInventory->SetVisibility(bSellMode ? ActiveVisibility : HiddenVisibility);
	}

	if (VBox_BuyLeft)
	{
		VBox_BuyLeft->SetVisibility(bSellMode ? HiddenVisibility : ActiveVisibility);
	}
	if (VBox_SellRoot)
	{
		VBox_SellRoot->SetVisibility(bSellMode ? ActiveVisibility : HiddenVisibility);
	}
}

void UShopPanelWidget::RefreshCurrencyText()
{
	if (!InventoryComponent) return;
	const int32 Currency = InventoryComponent->GetCurrency();
	const FText NumText  = FText::AsNumber(Currency);
	if (Text_Currency) Text_Currency->SetText(NumText);
	if (UTextBlock* InvCurrency = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_CurrencyInInventory"))))
	{
		FText Formatted = FText::Format(FText::FromString(TEXT("{0} G")), NumText);
		InvCurrency->SetText(Formatted);
	}
}

// ────────────────────────────────────────────────────────────────────────────
// 구매 탭 내부
// ────────────────────────────────────────────────────────────────────────────

void UShopPanelWidget::RefreshBuyList()
{
	if (!ScrollBox_BuyList || !ShopDefinition) return;
	ScrollBox_BuyList->ClearChildren();

	auto AddEntry = [&](FName RowName, const FRetrieveShopItemRow& Row, bool bRotating)
	{
		if (!ShopItemEntryWidgetClass) return;
		UShopItemEntryWidget* Entry = CreateWidget<UShopItemEntryWidget>(this, ShopItemEntryWidgetClass);
		if (!Entry) return;
		Entry->SetupBuyEntry(RowName, Row,
			GetItemDisplayName(Row.ItemId, Row.ItemCategoryTag),
			GetItemIcon(Row.ItemId));
		Entry->SetIsRotatingStock(bRotating);
		Entry->OnEntryClicked.AddDynamic(this, &UShopPanelWidget::SelectBuyItem);
		ScrollBox_BuyList->AddChild(Entry);
	};

	// ItemId 기준 중복 방지: ShopItemTable 과 RotatingPoolTable 에 동일 아이템이 있어도 1회만 표시
	TSet<FName> AddedItemIds;

	if (ShopDefinition->ShopItemTable)
	{
		TArray<FName> TargetRowNames = ShopDefinition->ShopItemRowFilter.Num() > 0
			? ShopDefinition->ShopItemRowFilter
			: ShopDefinition->ShopItemTable->GetRowNames();

		TArray<TPair<FName, FRetrieveShopItemRow*>> Pairs;
		for (FName RowName : TargetRowNames)
		{
			FRetrieveShopItemRow* Row = ShopDefinition->ShopItemTable->FindRow<FRetrieveShopItemRow>(
				RowName, TEXT("RefreshBuyList"));
			if (Row) Pairs.Add({RowName, Row});
		}
		Pairs.Sort([](const TPair<FName, FRetrieveShopItemRow*>& A,
		              const TPair<FName, FRetrieveShopItemRow*>& B)
		{ return A.Value->SortOrder < B.Value->SortOrder; });

		for (auto& Pair : Pairs)
		{
			if (!AddedItemIds.Contains(Pair.Value->ItemId))
			{
				AddEntry(Pair.Key, *Pair.Value, false);
				AddedItemIds.Add(Pair.Value->ItemId);
			}
		}
	}

	if (ShopComponentRef && ShopDefinition->RotatingPoolTable)
	{
		for (FName RotatingRowName : ShopComponentRef->CachedRotatingRows)
		{
			const FRetrieveShopItemRow* Row = ShopDefinition->RotatingPoolTable->FindRow<FRetrieveShopItemRow>(
				RotatingRowName, TEXT("RefreshBuyList_Rotating"));
			if (Row && !AddedItemIds.Contains(Row->ItemId))
			{
				AddEntry(RotatingRowName, *Row, true);
				AddedItemIds.Add(Row->ItemId);
			}
		}
	}
}

void UShopPanelWidget::RefreshBuyDetail()
{
	if (SelectedBuyRowName.IsNone() || !ShopDefinition) return;

	const FRetrieveShopItemRow* Row = FindShopRow(ShopDefinition, ShopComponentRef,
	                                               SelectedBuyRowName, TEXT("RefreshBuyDetail"));
	if (!Row) return;

	const float Multiplier = ShopDefinition ? ShopDefinition->PriceMultiplier : 1.0f;
	// 0원 상품은 무료 구매가 가능해야 한다. 잘못된 음수 값만 0으로 보정한다.
	SelectedBuyPrice = FMath::Max(0, FMath::RoundToInt(Row->BuyPrice * Multiplier));

	if (Image_ItemIcon)
	{
		if (UTexture2D* Icon = GetItemIcon(Row->ItemId))
			Image_ItemIcon->SetBrushFromTexture(Icon);
	}
	if (Text_ItemName)    Text_ItemName->SetText(GetItemDisplayName(Row->ItemId, Row->ItemCategoryTag));
	if (Text_ItemPrice)   Text_ItemPrice->SetText(FText::AsNumber(SelectedBuyPrice * BuyQuantity));
	if (Text_ItemDesc)
	{
		const FGameplayTag Cat = Row->ItemCategoryTag;
		static const FGameplayTag WeaponTag = FGameplayTag::RequestGameplayTag("Item.Weapon");
		static const FGameplayTag ArmorTag  = FGameplayTag::RequestGameplayTag("Item.Armor");
		static const FGameplayTag ConsumTag = FGameplayTag::RequestGameplayTag("Item.Consumable");

		auto TagLeaf = [](const FGameplayTag& T) -> FString
		{
			if (!T.IsValid()) return FString();
			FString S = T.ToString();
			int32 Idx;
			if (S.FindLastChar(TEXT('.'), Idx)) S = S.RightChop(Idx + 1);
			return S;
		};

		auto AddDetailRow = [](FString& Out, const TCHAR* Icon, const TCHAR* Label, const FString& Value)
		{
			if (!Value.IsEmpty())
			{
				Out += FString::Printf(TEXT("%s  %s  %s\n"), Icon, Label, *Value);
			}
		};

		FString Detail;
		if (Cat.MatchesTag(WeaponTag) && WeaponDataTable)
		{
			if (const FRetrieveWeaponDataRow* W = WeaponDataTable->FindRow<FRetrieveWeaponDataRow>(Row->ItemId, TEXT("ShopDetail")))
			{
				AddDetailRow(Detail, TEXT("◆"), TEXT("공격력"), FString::Printf(TEXT("%.0f"), W->AttackPower));
				if (W->WeaponTypeTag.IsValid())     AddDetailRow(Detail, TEXT("▣"), TEXT("종류"), TagLeaf(W->WeaponTypeTag));
				if (W->WeaponAffinityTag.IsValid()) AddDetailRow(Detail, TEXT("◇"), TEXT("속성"), TagLeaf(W->WeaponAffinityTag));
				if (!W->ShortDescription.IsEmpty()) AddDetailRow(Detail, TEXT("●"), TEXT("설명"), W->ShortDescription.ToString());
			}
		}
		else if (Cat.MatchesTag(ArmorTag) && ArmorDataTable)
		{
			if (const FRetrieveArmorDataRow* A = ArmorDataTable->FindRow<FRetrieveArmorDataRow>(Row->ItemId, TEXT("ShopDetail")))
			{
				AddDetailRow(Detail, TEXT("◆"), TEXT("방어력"), FString::Printf(TEXT("+%.0f"), A->Defense));
				if (A->EquipmentSlotTag.IsValid()) AddDetailRow(Detail, TEXT("▣"), TEXT("부위"), TagLeaf(A->EquipmentSlotTag));
				if (!A->ShortDescription.IsEmpty()) AddDetailRow(Detail, TEXT("●"), TEXT("설명"), A->ShortDescription.ToString());
			}
		}
		else if (Cat.MatchesTag(ConsumTag) && ConsumableItemTable)
		{
			if (const FRetrieveConsumableItemRow* C = ConsumableItemTable->FindRow<FRetrieveConsumableItemRow>(Row->ItemId, TEXT("ShopDetail")))
			{
				if (C->HealAmount > 0.f)            AddDetailRow(Detail, TEXT("◆"), TEXT("회복량"), FString::Printf(TEXT("%.0f"), C->HealAmount));
				if (C->ElementBuffMultiplier > 1.f) AddDetailRow(Detail, TEXT("◇"), TEXT("속성 충전"), FString::Printf(TEXT("x%.1f (%.0f초)"), C->ElementBuffMultiplier, C->BuffDuration));
				if (!C->ShortDescription.IsEmpty()) AddDetailRow(Detail, TEXT("●"), TEXT("설명"), C->ShortDescription.ToString());
			}
		}

		if (Detail.IsEmpty())
		{
			AddDetailRow(Detail, TEXT("●"), TEXT("설명"), GetItemShortDesc(Row->ItemId, Cat).ToString());
		}
		Text_ItemDesc->SetText(FText::FromString(Detail));
	}
	if (Text_BuyQuantity) Text_BuyQuantity->SetText(FText::AsNumber(BuyQuantity));

	if (Text_OwnedCount && InventoryComponent)
		Text_OwnedCount->SetText(FText::AsNumber(InventoryComponent->GetItemCount(Row->ItemId)));

	const float NewStat = GetItemStatValue(Row->ItemId, Row->ItemCategoryTag);
	if (Text_StatNew) Text_StatNew->SetText(FText::AsNumber(FMath::FloorToInt(NewStat)));

	FName EquippedId = InventoryComponent ? InventoryComponent->GetEquippedWeaponId() : NAME_None;
	const float CurStat = EquippedId.IsNone() ? 0.f : GetItemStatValue(EquippedId, Row->ItemCategoryTag);
	if (Text_StatCurrent) Text_StatCurrent->SetText(FText::AsNumber(FMath::FloorToInt(CurStat)));
}

// ────────────────────────────────────────────────────────────────────────────
// DataTable 헬퍼
// ────────────────────────────────────────────────────────────────────────────

int32 UShopPanelWidget::CalcSellPrice(FName ItemId, FGameplayTag CategoryTag) const
{
	if (!ShopDefinition) return 0;
	const int32 BasePrice = GetItemBasePrice(ItemId, CategoryTag);
	if (BasePrice <= 0) return 0;

	return FMath::Max(0, FMath::FloorToInt(BasePrice * ShopDefinition->SellPriceRate));
}

int32 UShopPanelWidget::GetItemBasePrice(FName ItemId, FGameplayTag CategoryTag) const
{
	static const FGameplayTag WeaponTag = FGameplayTag::RequestGameplayTag("Item.Weapon");
	static const FGameplayTag ArmorTag  = FGameplayTag::RequestGameplayTag("Item.Armor");
	static const FGameplayTag ConsumTag = FGameplayTag::RequestGameplayTag("Item.Consumable");
	static const FGameplayTag MaterialTag = FGameplayTag::RequestGameplayTag("Item.Material");

	if (CategoryTag.MatchesTag(WeaponTag) && WeaponDataTable)
	{
		const FRetrieveWeaponDataRow* Row = WeaponDataTable->FindRow<FRetrieveWeaponDataRow>(ItemId, TEXT(""));
		return Row ? Row->BasePrice : 0;
	}
	if (CategoryTag.MatchesTag(ArmorTag) && ArmorDataTable)
	{
		const FRetrieveArmorDataRow* Row = ArmorDataTable->FindRow<FRetrieveArmorDataRow>(ItemId, TEXT(""));
		return Row ? Row->BasePrice : 0;
	}
	if (CategoryTag.MatchesTag(ConsumTag) && ConsumableItemTable)
	{
		const FRetrieveConsumableItemRow* Row = ConsumableItemTable->FindRow<FRetrieveConsumableItemRow>(ItemId, TEXT(""));
		return Row ? Row->BasePrice : 0;
	}
	if (CategoryTag.MatchesTag(MaterialTag) && MaterialItemTable)
	{
		const FRetrieveMaterialItemRow* Row = MaterialItemTable->FindRow<FRetrieveMaterialItemRow>(ItemId, TEXT(""));
		return Row ? Row->BasePrice : 0;
	}
	return 0;
}

FText UShopPanelWidget::GetItemDisplayName(FName ItemId, FGameplayTag CategoryTag) const
{
	static const FGameplayTag WeaponTag = FGameplayTag::RequestGameplayTag("Item.Weapon");
	static const FGameplayTag ArmorTag  = FGameplayTag::RequestGameplayTag("Item.Armor");
	static const FGameplayTag ConsumTag = FGameplayTag::RequestGameplayTag("Item.Consumable");
	static const FGameplayTag MaterialTag = FGameplayTag::RequestGameplayTag("Item.Material");

	if (CategoryTag.MatchesTag(WeaponTag) && WeaponDataTable)
	{
		const FRetrieveWeaponDataRow* Row = WeaponDataTable->FindRow<FRetrieveWeaponDataRow>(ItemId, TEXT(""));
		if (Row) return Row->DisplayName;
	}
	if (CategoryTag.MatchesTag(ArmorTag) && ArmorDataTable)
	{
		const FRetrieveArmorDataRow* Row = ArmorDataTable->FindRow<FRetrieveArmorDataRow>(ItemId, TEXT(""));
		if (Row) return Row->DisplayName;
	}
	if (CategoryTag.MatchesTag(ConsumTag) && ConsumableItemTable)
	{
		const FRetrieveConsumableItemRow* Row = ConsumableItemTable->FindRow<FRetrieveConsumableItemRow>(ItemId, TEXT(""));
		if (Row) return Row->DisplayName;
	}
	if (CategoryTag.MatchesTag(MaterialTag) && MaterialItemTable)
	{
		const FRetrieveMaterialItemRow* Row = MaterialItemTable->FindRow<FRetrieveMaterialItemRow>(ItemId, TEXT(""));
		if (Row) return Row->DisplayName;
	}
	return FText::FromName(ItemId);
}

FText UShopPanelWidget::GetItemShortDesc(FName ItemId, FGameplayTag CategoryTag) const
{
	static const FGameplayTag WeaponTag = FGameplayTag::RequestGameplayTag("Item.Weapon");
	static const FGameplayTag ArmorTag  = FGameplayTag::RequestGameplayTag("Item.Armor");
	static const FGameplayTag ConsumTag = FGameplayTag::RequestGameplayTag("Item.Consumable");
	static const FGameplayTag MaterialTag = FGameplayTag::RequestGameplayTag("Item.Material");

	if (CategoryTag.MatchesTag(WeaponTag) && WeaponDataTable)
	{
		const FRetrieveWeaponDataRow* Row = WeaponDataTable->FindRow<FRetrieveWeaponDataRow>(ItemId, TEXT(""));
		if (Row) return Row->ShortDescription;
	}
	if (CategoryTag.MatchesTag(ArmorTag) && ArmorDataTable)
	{
		const FRetrieveArmorDataRow* Row = ArmorDataTable->FindRow<FRetrieveArmorDataRow>(ItemId, TEXT(""));
		if (Row) return Row->ShortDescription;
	}
	if (CategoryTag.MatchesTag(ConsumTag) && ConsumableItemTable)
	{
		const FRetrieveConsumableItemRow* Row = ConsumableItemTable->FindRow<FRetrieveConsumableItemRow>(ItemId, TEXT(""));
		if (Row) return Row->ShortDescription;
	}
	if (CategoryTag.MatchesTag(MaterialTag) && MaterialItemTable)
	{
		const FRetrieveMaterialItemRow* Row = MaterialItemTable->FindRow<FRetrieveMaterialItemRow>(ItemId, TEXT(""));
		if (Row) return Row->ShortDescription;
	}
	return FText::GetEmpty();
}

float UShopPanelWidget::GetItemStatValue(FName ItemId, FGameplayTag CategoryTag) const
{
	static const FGameplayTag WeaponTag = FGameplayTag::RequestGameplayTag("Item.Weapon");
	static const FGameplayTag ArmorTag  = FGameplayTag::RequestGameplayTag("Item.Armor");

	if (CategoryTag.MatchesTag(WeaponTag) && WeaponDataTable)
	{
		const FRetrieveWeaponDataRow* Row = WeaponDataTable->FindRow<FRetrieveWeaponDataRow>(ItemId, TEXT(""));
		return Row ? Row->AttackPower : 0.f;
	}
	if (CategoryTag.MatchesTag(ArmorTag) && ArmorDataTable)
	{
		const FRetrieveArmorDataRow* Row = ArmorDataTable->FindRow<FRetrieveArmorDataRow>(ItemId, TEXT(""));
		return Row ? Row->Defense : 0.f;
	}
	return 0.f;
}

UTexture2D* UShopPanelWidget::GetItemIcon(FName ItemId) const
{
	if (!ItemIconTable) return nullptr;
	const FRetrieveItemIconRow* Row = ItemIconTable->FindRow<FRetrieveItemIconRow>(ItemId, TEXT(""));
	return Row ? Row->IconTexture.LoadSynchronous() : nullptr;
}

// ────────────────────────────────────────────────────────────────────────────
// 버튼 바인딩 / 핸들러
// ────────────────────────────────────────────────────────────────────────────

void UShopPanelWidget::BindButtonEvents()
{
	if (Button_TabBuy)           Button_TabBuy->OnClicked.AddDynamic(this, &UShopPanelWidget::HandleTabBuyClicked);
	if (Button_TabSell)          Button_TabSell->OnClicked.AddDynamic(this, &UShopPanelWidget::HandleTabSellClicked);
	if (Button_Buy)              Button_Buy->OnClicked.AddDynamic(this, &UShopPanelWidget::HandleBuyButtonClicked);
	if (Button_BuyIncrease)      Button_BuyIncrease->OnClicked.AddDynamic(this, &UShopPanelWidget::HandleBuyIncreaseClicked);
	if (Button_BuyDecrease)      Button_BuyDecrease->OnClicked.AddDynamic(this, &UShopPanelWidget::HandleBuyDecreaseClicked);
	if (Button_SellTab_Weapon)   Button_SellTab_Weapon->OnClicked.AddDynamic(this, &UShopPanelWidget::HandleSellTabWeaponClicked);
	if (Button_SellTab_Consumable) Button_SellTab_Consumable->OnClicked.AddDynamic(this, &UShopPanelWidget::HandleSellTabConsumableClicked);
	if (Button_SellTab_Material) Button_SellTab_Material->OnClicked.AddDynamic(this, &UShopPanelWidget::HandleSellTabMaterialClicked);
	if (Button_SellTab_Armor)    Button_SellTab_Armor->OnClicked.AddDynamic(this, &UShopPanelWidget::HandleSellTabArmorClicked);
	if (Button_SellSelected)     Button_SellSelected->OnClicked.AddDynamic(this, &UShopPanelWidget::HandleSellSelectedClicked);
	if (Button_OpenRepurchase)   Button_OpenRepurchase->OnClicked.AddDynamic(this, &UShopPanelWidget::HandleOpenRepurchaseClicked);
	if (Button_BackToSell)       Button_BackToSell->OnClicked.AddDynamic(this, &UShopPanelWidget::HandleBackToSellClicked);
	if (Button_RepurchaseAll)    Button_RepurchaseAll->OnClicked.AddDynamic(this, &UShopPanelWidget::HandleRepurchaseAllClicked);
	if (UButton* CloseBtn = Cast<UButton>(GetWidgetFromName(TEXT("Button_Close"))))
		CloseBtn->OnClicked.AddDynamic(this, &UShopPanelWidget::HandleCloseShopClicked);
	if (CheckBox_SelectAll)      CheckBox_SelectAll->OnCheckStateChanged.AddDynamic(this, &UShopPanelWidget::HandleSelectAllChanged);
}

void UShopPanelWidget::HandleCloseShopClicked()
{
	if (ARetrievePlayerController* RetrievePC = Cast<ARetrievePlayerController>(GetOwningPlayer()))
	{
		if (RetrievePC->ReturnToShopConversation())
		{
			return;
		}
	}

	RequestClose();
}
void UShopPanelWidget::HandleTabBuyClicked()    { SwitchToBuyTab(); }
void UShopPanelWidget::HandleTabSellClicked()   { SwitchToSellTab(); }
void UShopPanelWidget::HandleBuyButtonClicked() { ExecuteBuy(); }

void UShopPanelWidget::HandleBuyIncreaseClicked()
{
	BuyQuantity = FMath::Min(BuyQuantity + 1, 99);
	RefreshBuyDetail();
}
void UShopPanelWidget::HandleBuyDecreaseClicked()
{
	BuyQuantity = FMath::Max(BuyQuantity - 1, 1);
	RefreshBuyDetail();
}

void UShopPanelWidget::RestoreSellGridView()
{
	if (Switcher_SellView) Switcher_SellView->SetActiveWidgetIndex(0);
	if (Border_ListHeader) Border_ListHeader->SetVisibility(ESlateVisibility::Visible);
	if (UWidget* W = GetWidgetFromName(TEXT("Overlay_85")))
		W->SetVisibility(ESlateVisibility::Visible);
}

void UShopPanelWidget::HandleSellTabWeaponClicked()
{
	static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag("Item.Weapon");
	RestoreSellGridView();
	RefreshSellGrid(Tag);
}
void UShopPanelWidget::HandleSellTabConsumableClicked()
{
	static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag("Item.Consumable");
	RestoreSellGridView();
	RefreshSellGrid(Tag);
}
void UShopPanelWidget::HandleSellTabMaterialClicked()
{
	static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag("Item.Material");
	RestoreSellGridView();
	RefreshSellGrid(Tag);
}
void UShopPanelWidget::HandleSellTabArmorClicked()
{
	static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag("Item.Armor");
	RestoreSellGridView();
	RefreshSellGrid(Tag);
}

void UShopPanelWidget::HandleSellSelectedClicked()   { ExecuteSellSelected(); }

void UShopPanelWidget::HandleOpenRepurchaseClicked()
{
	RefreshRepurchaseList();
	if (Switcher_SellView) Switcher_SellView->SetActiveWidgetIndex(1);
	if (Border_ListHeader) Border_ListHeader->SetVisibility(ESlateVisibility::Collapsed);
	if (UWidget* W = GetWidgetFromName(TEXT("Overlay_85")))
		W->SetVisibility(ESlateVisibility::Collapsed);
}

void UShopPanelWidget::HandleBackToSellClicked()
{
	RestoreSellGridView();
}

void UShopPanelWidget::HandleRepurchaseAllClicked()  { ExecuteRepurchaseAll(); }

void UShopPanelWidget::HandleSelectAllChanged(bool bIsChecked)
{
	SelectedSlotIndices.Empty();
	if (bIsChecked)
	{
		for (int32 i = 0; i < CurrentSellSlots.Num(); ++i)
			SelectedSlotIndices.Add(i);
	}
	UpdateSellSlotVisuals();
	UpdateSellToolbar();
}

void UShopPanelWidget::HandleCurrencyChanged(int32)  { RefreshCurrencyText(); }

void UShopPanelWidget::HandleInventoryChanged()
{
	if (bSellModeActive)
	{
		RefreshSellGrid(CurrentSellCategoryTag);
	}
	else
	{
		RefreshBuyDetail();
	}
}
