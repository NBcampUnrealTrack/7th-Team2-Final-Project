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

	// 재고 확인(무한 재고면 -1이라 통과). 재화 차감 전에 검사해 재고 부족 시 돈이 빠지지 않게 한다.
	const int32 Remaining = ShopComponentRef
		? ShopComponentRef->GetRemainingStock(SelectedBuyRowName)
		: -1;
	if (Remaining >= 0 && Remaining < BuyQuantity)
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

	// 재화 차감이 확정된 뒤 재고를 차감한다(세이브에 지속). 무한 재고면 no-op.
	if (ShopComponentRef)
	{
		ShopComponentRef->ConsumeStock(SelectedBuyRowName, BuyQuantity);
	}

	InventoryComponent->AddItem(Row->ItemId, Row->ItemCategoryTag, BuyQuantity);
	OnItemPurchased.Broadcast(Row->ItemId, BuyQuantity);

	ShowShopDialogue(DialogueRow_BuySuccess, INVTEXT("좋은 선택이야. 잘 써."));
	RefreshCurrencyText();
	RefreshBuyList();   // 남은 재고 텍스트 갱신
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
	SelectedSlotQuantities.Empty();
	ActiveSellSlotIndex  = -1;
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
		Cache.ItemId         = Stack.ItemId;
		Cache.CategoryTag    = Stack.ItemCategoryTag;
		Cache.Quantity       = Stack.Quantity;
		Cache.SlotInstanceId = Stack.SlotInstanceId;
		Cache.bEquipped      = IsStackEquipped(Stack);
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
					Cache.bEquipped,
					CalcSellPrice(Stack.ItemId, Stack.ItemCategoryTag));
					// 아이콘/등급/스탯/판매가/뱃지를 채운 상세 툴팁을 패널이 생성해 주입.
					if (UUserWidget* Tip = BuildSellSlotTooltip(Cache))
					{
						SlotWidget->SetToolTip(Tip);
					}
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

void UShopPanelWidget::AddSlotToCart(int32 SlotIndex, bool bMakeActive)
{
	if (!CurrentSellSlots.IsValidIndex(SlotIndex)) return;
	if (CurrentSellSlots[SlotIndex].bEquipped) return;   // 장착 장비는 카트에 담지 않는다.

	SelectedSlotIndices.Add(SlotIndex);
	if (!SelectedSlotQuantities.Contains(SlotIndex))
	{
		// 기본 판매 수량 = 전량.
		SelectedSlotQuantities.Add(SlotIndex, FMath::Max(1, CurrentSellSlots[SlotIndex].Quantity));
	}
	if (bMakeActive) ActiveSellSlotIndex = SlotIndex;
}

void UShopPanelWidget::HandleSellSlotPressed(int32 SlotIndex, bool bShiftHeld)
{
	if (!CurrentSellSlots.IsValidIndex(SlotIndex)) return;

	bIsDragSelecting  = true;
	bDragIsShiftDrag  = bShiftHeld;

	if (bShiftHeld && LastClickedSlotIndex >= 0)
	{
		// Shift+클릭/드래그: 앵커~현재 범위를 모두 카트에 담는다(누적).
		DragStartSlotIndex = LastClickedSlotIndex;
		SelectRangeInSellGrid(LastClickedSlotIndex, SlotIndex);
	}
	else
	{
		DragStartSlotIndex   = SlotIndex;
		LastClickedSlotIndex = SlotIndex;

		if (SelectedSlotIndices.Contains(SlotIndex))
		{
			// 이미 담긴 슬롯: 활성이면 카트에서 빼고, 아니면 활성으로만 전환.
			if (ActiveSellSlotIndex == SlotIndex)
			{
				SelectedSlotIndices.Remove(SlotIndex);
				SelectedSlotQuantities.Remove(SlotIndex);
				ActiveSellSlotIndex = -1;
			}
			else
			{
				ActiveSellSlotIndex = SlotIndex;
			}
		}
		else
		{
			// 새로 담기(기본 전량) + 활성 지정.
			AddSlotToCart(SlotIndex, /*bMakeActive*/ true);
		}
	}

	UpdateSellSlotVisuals();
	UpdateSellToolbar();
}

void UShopPanelWidget::HandleSellSlotEntered(int32 SlotIndex)
{
	// Shift+drag 중일 때만 범위 누적
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
	for (int32 i = Start; i <= End && i < CurrentSellSlots.Num(); ++i)
	{
		AddSlotToCart(i, /*bMakeActive*/ false);
	}
	// 드래그 종료 슬롯을 활성으로(장착이 아니면).
	if (CurrentSellSlots.IsValidIndex(B) && !CurrentSellSlots[B].bEquipped)
	{
		ActiveSellSlotIndex = B;
	}
}

void UShopPanelWidget::UpdateSellSlotVisuals()
{
	for (int32 i = 0; i < SellSlotWidgets.Num(); ++i)
	{
		if (!SellSlotWidgets[i]) continue;
		const bool bSel    = SelectedSlotIndices.Contains(i);
		const bool bActive = (i == ActiveSellSlotIndex);
		const int32* QtyPtr = SelectedSlotQuantities.Find(i);
		SellSlotWidgets[i]->SetSelected(bSel, bActive, QtyPtr ? *QtyPtr : -1);
	}
}

void UShopPanelWidget::UpdateSellToolbar()
{
	const int32 SelectedCount = SelectedSlotIndices.Num();

	// ── 활성 슬롯 수량 ± UI: 활성 슬롯이 카트에 있고 보유량 2+ 일 때만 노출 ──
	const bool bActiveValid = CurrentSellSlots.IsValidIndex(ActiveSellSlotIndex)
		&& SelectedSlotQuantities.Contains(ActiveSellSlotIndex);
	const bool bShowQtyControls = bActiveValid && CurrentSellSlots[ActiveSellSlotIndex].Quantity > 1;
	const ESlateVisibility QtyVis =
		bShowQtyControls ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (Panel_SellQuantity)   Panel_SellQuantity->SetVisibility(QtyVis);
	if (Button_SellDecrease)  Button_SellDecrease->SetVisibility(QtyVis);
	if (Button_SellIncrease)  Button_SellIncrease->SetVisibility(QtyVis);
	if (Text_SellQuantity)
	{
		Text_SellQuantity->SetVisibility(QtyVis);
		if (bShowQtyControls)
		{
			Text_SellQuantity->SetText(FText::AsNumber(SelectedSlotQuantities[ActiveSellSlotIndex]));
		}
	}

	// ── 합계 금액: 담긴 각 슬롯의 판매 예정 수량 × 개당가 ──
	int32 TotalPrice = 0;
	for (const TPair<int32, int32>& Pair : SelectedSlotQuantities)
	{
		if (CurrentSellSlots.IsValidIndex(Pair.Key))
		{
			const FSellSlotCache& S = CurrentSellSlots[Pair.Key];
			TotalPrice += CalcSellPrice(S.ItemId, S.CategoryTag) * FMath::Max(0, Pair.Value);
		}
	}

	if (Text_SelectedCount)
	{
		Text_SelectedCount->SetText(SelectedCount > 0
			? FText::Format(INVTEXT("{0}종 선택"), FText::AsNumber(SelectedCount))
			: INVTEXT("선택 없음"));
	}

	if (Text_SelectedCurrency)
	{
		Text_SelectedCurrency->SetText(SelectedCount > 0
			? FText::Format(INVTEXT("{0} G"), FText::AsNumber(TotalPrice))
			: INVTEXT("0 G"));
	}

	if (Text_SellSelectionSummary)
	{
		if (SelectedCount <= 0)
		{
			Text_SellSelectionSummary->SetText(INVTEXT("판매할 품목을 선택하세요"));
		}
		else
		{
			TArray<int32> SortedIndices = SelectedSlotIndices.Array();
			SortedIndices.Sort();

			FString ItemSummary;
			int32 TotalQuantity = 0;
			int32 DisplayedCount = 0;
			for (const int32 SlotIndex : SortedIndices)
			{
				if (!CurrentSellSlots.IsValidIndex(SlotIndex)) continue;

				const FSellSlotCache& SellSlot = CurrentSellSlots[SlotIndex];
				const int32 SellQuantity = FMath::Clamp(
					SelectedSlotQuantities.FindRef(SlotIndex), 1, SellSlot.Quantity);
				TotalQuantity += SellQuantity;

				if (DisplayedCount < 3)
				{
					if (!ItemSummary.IsEmpty()) ItemSummary += TEXT("  ·  ");
					ItemSummary += FString::Printf(TEXT("%s %d/%d개"),
						*GetItemDisplayName(SellSlot.ItemId, SellSlot.CategoryTag).ToString(),
						SellQuantity, SellSlot.Quantity);
					++DisplayedCount;
				}
			}

			if (SelectedCount > DisplayedCount)
			{
				ItemSummary += FString::Printf(TEXT("  ·  외 %d종"), SelectedCount - DisplayedCount);
			}

			Text_SellSelectionSummary->SetText(FText::FromString(FString::Printf(
				TEXT("판매 목록  %s\n총 %d종 / %d개    |    예상 %d G"),
				*ItemSummary, SelectedCount, TotalQuantity, TotalPrice)));
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
		// 판매 가능한(장착 제외) 슬롯 수와 담긴 수를 비교.
		int32 Sellable = 0;
		for (const FSellSlotCache& S : CurrentSellSlots)
		{
			if (!S.bEquipped) ++Sellable;
		}
		const bool bAll = Sellable > 0 && SelectedSlotIndices.Num() == Sellable;
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

	// 선택 슬롯을 값으로 복사한다.
	// RemoveItem은 OnInventoryChanged를 브로드캐스트하고, 그 콜백(HandleInventoryChanged)이
	// 판매 모드에서 RefreshSellGrid를 호출해 CurrentSellSlots를 Empty()로 비운다.
	// 원본 배열의 참조(const FSellSlotCache&)를 들고 루프를 돌면 이 시점에 dangling reference가
	// 되어 크래시하므로, 판매 대상을 미리 값 복사한 뒤 그 복사본으로 실행한다.
	// 카트에 담긴 각 슬롯을 "선택 수량"만큼 판매한다(여러 종류를 각각 다른 수량으로 한 번에).
	// RemoveItem이 OnInventoryChanged→RefreshSellGrid로 CurrentSellSlots를 비우므로
	// 판매 대상을 미리 값 복사(수량 포함)한 뒤 복사본으로 실행한다(dangling ref 방지).
	TArray<FSellSlotCache> SlotsToSell;
	bool bSkippedEquipped = false;
	{
		TArray<int32> SortedIndices = SelectedSlotIndices.Array();
		SortedIndices.Sort([](int32 A, int32 B) { return A > B; });
		for (int32 Idx : SortedIndices)
		{
			if (!CurrentSellSlots.IsValidIndex(Idx))
			{
				continue;
			}
			// 장착 중인 장비는 판매 대상에서 제외한다(실수 판매 방지).
			if (CurrentSellSlots[Idx].bEquipped)
			{
				bSkippedEquipped = true;
				continue;
			}
			FSellSlotCache Copy = CurrentSellSlots[Idx];
			// 선택 수량으로 덮어쓴다(카트에 없으면 전량).
			const int32* QtyPtr = SelectedSlotQuantities.Find(Idx);
			Copy.Quantity = FMath::Clamp(QtyPtr ? *QtyPtr : Copy.Quantity, 1, Copy.Quantity);
			SlotsToSell.Add(Copy);
		}
	}

	// 선택이 전부 장착 장비여서 팔 게 없으면 안내하고 종료.
	if (SlotsToSell.Num() == 0)
	{
		if (bSkippedEquipped)
		{
			ShowShopDialogue(DialogueRow_SellFail_NoSelection,
				INVTEXT("장착 중인 장비는 팔 수 없어. 먼저 장비를 해제해줘."));
		}
		else
		{
			ShowShopDialogue(DialogueRow_SellFail_NoSelection, INVTEXT("팔 물건을 먼저 골라줘."));
		}
		return false;
	}

	// 판매 루프 도중 인벤토리 변경 콜백이 그리드를 즉시 재생성하지 않도록 가드한다.
	// 루프가 끝난 뒤 아래에서 한 번만 RefreshSellGrid를 호출한다.
	bIsSelling = true;

	bool bAnySold = false;
	for (const FSellSlotCache& S : SlotsToSell)
	{
		const int32 Price = CalcSellPrice(S.ItemId, S.CategoryTag) * S.Quantity;
		if (InventoryComponent->RemoveItem(S.ItemId, S.CategoryTag, S.Quantity, S.SlotInstanceId))
		{
			InventoryComponent->AddCurrency(Price);
			PushRepurchaseRecord(S.ItemId, S.CategoryTag, S.Quantity, Price);
			OnItemSold.Broadcast(S.ItemId, S.Quantity);
			bAnySold = true;
		}
	}

	bIsSelling = false;

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
	const FText GoldText = FText::Format(FText::FromString(TEXT("{0} G")), NumText);
	if (Text_BuyCurrentGold)
		Text_BuyCurrentGold->SetText(FText::Format(INVTEXT("보유 골드  {0} G"), NumText));
	if (UTextBlock* InvCurrency = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_CurrencyInInventory"))))
	{
		InvCurrency->SetText(GoldText);
	}
}

int32 UShopPanelWidget::GetBuyStockLimit() const
{
	// 무한 재고(또는 컴포넌트 없음) → 99 상한. 유한 재고면 남은 수량(최대 99).
	if (!ShopComponentRef || SelectedBuyRowName.IsNone())
	{
		return 99;
	}
	const int32 Remaining = ShopComponentRef->GetRemainingStock(SelectedBuyRowName);
	if (Remaining < 0)
	{
		return 99;
	}
	return FMath::Clamp(Remaining, 0, 99);
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
		// 정적 StockCount 대신 실제 남은 재고(구매로 줄어든 값)로 덮어쓴다.
		Entry->SetRuntimeStock(ShopComponentRef
			? ShopComponentRef->GetRemainingStock(RowName)
			: Row.StockCount);
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

	// 재고를 초과하는 수량은 설정할 수 없다(유한 재고 상점). 무한 재고면 99 상한.
	const int32 StockLimit = GetBuyStockLimit();
	BuyQuantity = FMath::Clamp(BuyQuantity, 1, FMath::Max(1, StockLimit));

	const int32 TotalCost = SelectedBuyPrice * BuyQuantity;

	if (Image_ItemIcon)
	{
		if (UTexture2D* Icon = GetItemIcon(Row->ItemId))
			Image_ItemIcon->SetBrushFromTexture(Icon);
	}
	if (Text_ItemName)    Text_ItemName->SetText(GetItemDisplayName(Row->ItemId, Row->ItemCategoryTag));
	if (Text_ItemPrice)   Text_ItemPrice->SetText(FText::AsNumber(TotalCost));
	if (Text_BuyTotalCost)
		Text_BuyTotalCost->SetText(FText::Format(INVTEXT("구매 예정  {0} G"), FText::AsNumber(TotalCost)));
	// 재고 상한에 도달하면 증가 버튼 비활성(무한 재고면 99에서만 비활성).
	if (Button_BuyIncrease) Button_BuyIncrease->SetIsEnabled(BuyQuantity < FMath::Max(1, StockLimit));
	if (Button_BuyDecrease) Button_BuyDecrease->SetIsEnabled(BuyQuantity > 1);
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

bool UShopPanelWidget::IsStackEquipped(const FRetrieveItemStack& Stack) const
{
	if (!InventoryComponent)
	{
		return false;
	}

	static const FGameplayTag WeaponTag = FGameplayTag::RequestGameplayTag("Item.Weapon");
	static const FGameplayTag ArmorTag  = FGameplayTag::RequestGameplayTag("Item.Armor");

	if (Stack.ItemCategoryTag.MatchesTag(WeaponTag))
	{
		// 무기: 정확히 장착된 슬롯(같은 ItemId가 여러 슬롯이어도 장착 슬롯만) 판정.
		return Stack.ItemId == InventoryComponent->GetEquippedWeaponId()
			&& Stack.SlotInstanceId != INDEX_NONE
			&& Stack.SlotInstanceId == InventoryComponent->GetEquippedWeaponSlotInstanceId();
	}

	if (Stack.ItemCategoryTag.MatchesTag(ArmorTag))
	{
		// 방어구: 장착 슬롯 목록에서 같은 ItemId + 같은 SlotInstanceId를 찾는다.
		for (const FRetrieveEquippedArmorEntry& E : InventoryComponent->GetEquippedArmorSlots())
		{
			if (E.ArmorItemId == Stack.ItemId
				&& Stack.SlotInstanceId != INDEX_NONE
				&& E.SlotInstanceId == Stack.SlotInstanceId)
			{
				return true;
			}
		}
	}

	return false;
}

UUserWidget* UShopPanelWidget::BuildSellSlotTooltip(const FSellSlotCache& SlotData) const
{
	if (!ItemDetailTooltipClass || SlotData.ItemId.IsNone())
	{
		return nullptr;
	}

	APlayerController* PC = GetOwningPlayer();
	UUserWidget* Tip = PC
		? CreateWidget<UUserWidget>(PC, ItemDetailTooltipClass)
		: CreateWidget<UUserWidget>(const_cast<UShopPanelWidget*>(this), ItemDetailTooltipClass);
	if (!Tip)
	{
		return nullptr;
	}
	Tip->SetToolTipText(FText::GetEmpty());
	Tip->SetToolTip(nullptr);

	auto SetTxt = [Tip](const TCHAR* Name, const FText& Value)
	{
		if (UTextBlock* T = Cast<UTextBlock>(Tip->GetWidgetFromName(Name)))
		{
			T->SetText(Value);
		}
	};
	auto SetVis = [Tip](const TCHAR* Name, bool bVisible)
	{
		if (UWidget* W = Tip->GetWidgetFromName(Name))
		{
			W->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	};

	static const FGameplayTag WeaponTag   = FGameplayTag::RequestGameplayTag("Item.Weapon");
	static const FGameplayTag ArmorTag    = FGameplayTag::RequestGameplayTag("Item.Armor");
	static const FGameplayTag ConsumTag   = FGameplayTag::RequestGameplayTag("Item.Consumable");
	static const FGameplayTag MaterialTag = FGameplayTag::RequestGameplayTag("Item.Material");

	FText RarityText;
	FText MainStatText;
	if (SlotData.CategoryTag.MatchesTag(WeaponTag))
	{
		RarityText = INVTEXT("무기");
		MainStatText = FText::Format(INVTEXT("공격력 {0}"),
			FText::AsNumber(FMath::FloorToInt(GetItemStatValue(SlotData.ItemId, SlotData.CategoryTag))));
	}
	else if (SlotData.CategoryTag.MatchesTag(ArmorTag))
	{
		RarityText = INVTEXT("방어구");
		MainStatText = FText::Format(INVTEXT("방어력 {0}"),
			FText::AsNumber(FMath::FloorToInt(GetItemStatValue(SlotData.ItemId, SlotData.CategoryTag))));
	}
	else if (SlotData.CategoryTag.MatchesTag(ConsumTag))
	{
		RarityText = INVTEXT("소모품");
		MainStatText = FText::Format(INVTEXT("{0}개 보유"), FText::AsNumber(SlotData.Quantity));
	}
	else if (SlotData.CategoryTag.MatchesTag(MaterialTag))
	{
		RarityText = INVTEXT("재료");
		MainStatText = FText::Format(INVTEXT("{0}개 보유"), FText::AsNumber(SlotData.Quantity));
	}

	// 뱃지: 장착 중일 때만 노출(기본 텍스트 "장착됨"이 항상 보이던 문제 해결).
	SetTxt(TEXT("Text_Badge"), SlotData.bEquipped ? INVTEXT("장착 중") : FText::GetEmpty());
	SetVis(TEXT("Text_Badge"), SlotData.bEquipped);
	SetVis(TEXT("IMG_BadgeFrame"), SlotData.bEquipped);

	SetTxt(TEXT("Text_ItemName"), GetItemDisplayName(SlotData.ItemId, SlotData.CategoryTag));
	SetTxt(TEXT("Text_ItemRarity"), RarityText);
	SetTxt(TEXT("Text_MainStat"), MainStatText);
	SetTxt(TEXT("Text_ItemDetails"), GetItemShortDesc(SlotData.ItemId, SlotData.CategoryTag));
	SetTxt(TEXT("Text_CurrencyValue"), FText::AsNumber(CalcSellPrice(SlotData.ItemId, SlotData.CategoryTag)));

	if (UImage* IconImg = Cast<UImage>(Tip->GetWidgetFromName(TEXT("Image_ItemIcon"))))
	{
		if (UTexture2D* Tex = GetItemIcon(SlotData.ItemId))
		{
			IconImg->SetBrushFromTexture(Tex);
		}
	}

	return Tip;
}

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
	if (Button_SellDecrease)     Button_SellDecrease->OnClicked.AddDynamic(this, &UShopPanelWidget::HandleSellDecreaseClicked);
	if (Button_SellIncrease)     Button_SellIncrease->OnClicked.AddDynamic(this, &UShopPanelWidget::HandleSellIncreaseClicked);
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
	// 남은 재고(무한이면 99)를 넘겨 늘릴 수 없다.
	const int32 StockLimit = FMath::Max(1, GetBuyStockLimit());
	BuyQuantity = FMath::Min(BuyQuantity + 1, StockLimit);
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

void UShopPanelWidget::HandleSellDecreaseClicked()
{
	if (!SelectedSlotQuantities.Contains(ActiveSellSlotIndex)) return;
	int32& Qty = SelectedSlotQuantities[ActiveSellSlotIndex];
	Qty = FMath::Max(1, Qty - 1);
	UpdateSellSlotVisuals();   // 슬롯에 표시된 판매 예정 수량도 갱신
	UpdateSellToolbar();
}

void UShopPanelWidget::HandleSellIncreaseClicked()
{
	if (!SelectedSlotQuantities.Contains(ActiveSellSlotIndex)) return;
	if (!CurrentSellSlots.IsValidIndex(ActiveSellSlotIndex)) return;
	const int32 SlotQty = CurrentSellSlots[ActiveSellSlotIndex].Quantity;
	int32& Qty = SelectedSlotQuantities[ActiveSellSlotIndex];
	Qty = FMath::Min(SlotQty, Qty + 1);
	UpdateSellSlotVisuals();
	UpdateSellToolbar();
}

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
	SelectedSlotQuantities.Empty();
	ActiveSellSlotIndex = -1;
	if (bIsChecked)
	{
		// 판매 가능한(장착 제외) 슬롯을 전부 전량으로 담는다.
		for (int32 i = 0; i < CurrentSellSlots.Num(); ++i)
		{
			AddSlotToCart(i, /*bMakeActive*/ false);
		}
	}
	UpdateSellSlotVisuals();
	UpdateSellToolbar();
}

void UShopPanelWidget::HandleCurrencyChanged(int32)  { RefreshCurrencyText(); }

void UShopPanelWidget::HandleInventoryChanged()
{
	// 판매 루프 진행 중에는 그리드를 재생성하지 않는다(ExecuteSellSelected가 끝난 뒤 1회 갱신).
	// 루프 도중 재생성하면 판매 중인 슬롯 캐시가 무효화돼 크래시한다.
	if (bIsSelling)
	{
		return;
	}

	if (bSellModeActive)
	{
		RefreshSellGrid(CurrentSellCategoryTag);
	}
	else
	{
		RefreshBuyDetail();
	}
}
