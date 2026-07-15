#pragma once

#include "CoreMinimal.h"
#include "UI/RetrieveGamePanelWidget.h"
#include "Shop/RetrieveShopTypes.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "ShopPanelWidget.generated.h"

class UInventoryComponent;
class URetrieveShopComponent;
class URetrieveShopDefinitionAsset;
class UScrollBox;
class UTextBlock;
class UImage;
class UButton;
class UBorder;
class UWidget;
class UWidgetSwitcher;
class UUniformGridPanel;
class UCheckBox;
class UHorizontalBox;
class UVerticalBox;
class UDataTable;
class UShopItemEntryWidget;
class UShopSellSlotWidget;
class UShopRepurchaseEntryWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShopTransactionSignature, FName, ItemId, int32, Amount);


/** ShopPanel 전용 대사 CSV/DataTable row.
 *  기존 DT_Dialogue와 분리해서 사용한다.
 *  이 테이블은 NPC 대화 선택지 생성 시스템에서 읽지 않고, WBP_ShopPanel의 TXT_Dialogue 표시용으로만 사용한다.
 *
 *  CSV 컬럼 예시:
 *  ---,SpeakerName,DialogueText
 *  ShopNPC_Greeting,상인,어서 와. 필요한 걸 골라봐.
 */
USTRUCT(BlueprintType)
struct FRetrieveShopDialogueRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 필요하면 상점 패널에서 NPC 이름 표시용으로 사용할 수 있는 이름. 비어 있으면 ShopDefinition->ShopName을 사용한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText SpeakerName;

	/** 상점 패널 TXT_Dialogue에 표시할 실제 대사 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText DialogueText;
};

/**
 * 상점 UI 메인 패널.
 *
 * ─ 공통 헤더 ─────────────────────────────────────────────────────────────────
 *   Button_TabBuy / Button_TabSell,  Text_Currency (우상단)
 *   Switcher_Tabs (인덱스 0 = 구매, 1 = 판매)
 *
 * ─ 구매 탭 (index 0) ─────────────────────────────────────────────────────────
 *   좌측: ScrollBox_BuyList / 우측: Panel_BuyDetail
 *
 * ─ 판매 탭 (index 1) ─────────────────────────────────────────────────────────
 *   [카테고리 탭] Button_SellTab_Weapon/Consumable/Material/Armor
 *   [툴바]        CheckBox_SelectAll  |  Text_SelectedCount  |  Button_SellSelected  |  Button_OpenRepurchase
 *   Switcher_SellView (0 = 아이템 그리드, 1 = 재구매 목록)
 *     [View 0] ScrollBox_SellGrid  →  WrapBox_SellItems  →  WBP_ShopSellSlot
 *     [View 1] Button_BackToSell / ScrollBox_RepurchaseList / Button_RepurchaseAll / Text_RepurchaseTotalCost
 *
 * ─ shift+drag 다중선택 ────────────────────────────────────────────────────────
 *   WBP_ShopSellSlot 이벤트 → HandleSellSlotPressed / HandleSellSlotEntered / EndDragSelect
 */
UCLASS()
class RETRIEVE_API UShopPanelWidget : public URetrieveGamePanelWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Shop")
	void InitializeShopPanel(UInventoryComponent* InInventoryComp,
	                         URetrieveShopDefinitionAsset* InShopDefinition,
	                         URetrieveShopComponent* InShopComponent = nullptr);

	// ── 탭 전환 ────────────────────────────────────────────────────────────────
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Shop")
	void SwitchToBuyTab();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Shop")
	void SwitchToSellTab();

	// ── 구매 탭 ────────────────────────────────────────────────────────────────
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Shop")
	void SelectBuyItem(FName ShopRowName);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Shop")
	bool ExecuteBuy();

	UFUNCTION(BlueprintPure, Category = "Retrieve|Shop")
	bool CanExecuteBuy() const;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Shop")
	int32 GetBuyQuantity() const { return BuyQuantity; }

	// ── 판매 탭 ── 다중선택 ────────────────────────────────────────────────────
	/** WBP_ShopSellSlot 에서 호출 (마우스 버튼 다운) */
	void HandleSellSlotPressed(int32 SlotIndex, bool bShiftHeld);

	/** WBP_ShopSellSlot 에서 호출 (마우스 버튼 누른 채 진입) */
	void HandleSellSlotEntered(int32 SlotIndex);

	/** 마우스 버튼 업 시 드래그 종료 */
	void EndDragSelect();

	/** 선택된 아이템 전량 판매 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Shop")
	bool ExecuteSellSelected();

	UFUNCTION(BlueprintPure, Category = "Retrieve|Shop")
	bool CanExecuteSellSelected() const;

	// ── 재구매 ─────────────────────────────────────────────────────────────────
	/** 히스토리 인덱스의 항목 1건 재구매. ShopRepurchaseEntryWidget 에서 호출 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Shop")
	void ExecuteRepurchase(int32 HistoryIndex);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Shop")
	void ExecuteRepurchaseAll();

	// ── DataTable / Widget 클래스 참조 ─────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Shop|Data")
	TObjectPtr<UDataTable> ItemIconTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Shop|Data")
	TObjectPtr<UDataTable> WeaponDataTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Shop|Data")
	TObjectPtr<UDataTable> ArmorDataTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Shop|Data")
	TObjectPtr<UDataTable> ConsumableItemTable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Shop|Data")
	TObjectPtr<UDataTable> MaterialItemTable;

	/** 상점 패널 전용 대사 테이블. 기존 DT_Dialogue가 아니라 별도 DT_ShopDialogue를 만들어 할당한다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Shop|Dialogue")
	TObjectPtr<UDataTable> ShopDialogueTable;

	/** 대사 테이블 RowName 설정 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Shop|Dialogue|Rows")
	FName DialogueRow_Greeting = TEXT("ShopNPC_Greeting");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Shop|Dialogue|Rows")
	FName DialogueRow_BuySuccess = TEXT("ShopNPC_BuySuccess");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Shop|Dialogue|Rows")
	FName DialogueRow_BuyFail_NotEnoughMoney = TEXT("ShopNPC_BuyFail_NotEnoughMoney");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Shop|Dialogue|Rows")
	FName DialogueRow_BuyFail_OutOfStock = TEXT("ShopNPC_BuyFail_OutOfStock");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Shop|Dialogue|Rows")
	FName DialogueRow_SellSuccess = TEXT("ShopNPC_SellSuccess");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Shop|Dialogue|Rows")
	FName DialogueRow_SellFail_MerchantNoMoney = TEXT("ShopNPC_SellFail_MerchantNoMoney");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Shop|Dialogue|Rows")
	FName DialogueRow_SellFail_NoSelection = TEXT("ShopNPC_SellFail_NoSelection");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Shop|Data")
	TSubclassOf<UShopItemEntryWidget> ShopItemEntryWidgetClass;

	/** WBP_ShopSellSlot 클래스 (Details 패널에서 할당) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Shop|Data")
	TSubclassOf<UShopSellSlotWidget> SellSlotWidgetClass;

	/** WBP_ShopRepurchaseEntry 클래스 (Details 패널에서 할당) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Shop|Data")
	TSubclassOf<UShopRepurchaseEntryWidget> RepurchaseEntryWidgetClass;

	/** WBP_ItemDetailTooltip 클래스 (Details 패널에서 할당) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Shop|Data")
	TSubclassOf<UUserWidget> ItemDetailTooltipClass;

	/** 판매 그리드 열 수 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Shop", meta = (ClampMin = "1"))
	int32 SellGridColumns = 4;

	/** 판매 슬롯 최소 크기 (너무 작게 생성되는 것 방지) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Shop", meta = (ClampMin = "40"))
	FVector2D SellSlotMinSize = FVector2D(100.0f, 100.0f);

	/** 항상 표시할 최소 행 수 (빈 슬롯 포함) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Shop", meta = (ClampMin = "1"))
	int32 SellGridMinRows = 4;

	/** 재구매 히스토리 최대 보관 건수 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Shop", meta = (ClampMin = "1"))
	int32 MaxRepurchaseHistory = 20;

	// ── 이벤트 ────────────────────────────────────────────────────────────────
	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Shop|Events")
	FShopTransactionSignature OnItemPurchased;

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Shop|Events")
	FShopTransactionSignature OnItemSold;

protected:
	virtual void NativeConstruct() override;

	// ── 공통 ───────────────────────────────────────────────────────────────────
	/** 탭 버튼 헤더 (3열 레이아웃에서 숨김 처리) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> HBox_Header;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> Switcher_Tabs;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_TabBuy;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_TabSell;

	/** 좌열: 구매 목록 컨테이너 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> VBox_BuyLeft;

	/** 우열: 인벤토리 그리드 컨테이너 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> VBox_SellRoot;

	/** 판매 그리드 영역 흰 배경 Border (투명화 대상) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_SellGridBg;

	/** 판매 뷰 컬럼 헤더 (재구매 뷰 진입 시 숨김) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> Border_ListHeader;

	// Button_Close: WBP 위젯. GetWidgetFromName(TEXT("Button_Close"))로 바인딩 (BindWidget 충돌 방지로 C++ 멤버 없음)

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ShopTypeBadge;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_TabBuyLabel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_TabSellLabel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_Currency;

	// Text_CurrencyInInventory: WBP 위젯. GetWidgetFromName으로 접근 (C++ 멤버 없음)

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_NpcName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_NpcDialogue;

	// ── 구매 탭 ────────────────────────────────────────────────────────────────
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> ScrollBox_BuyList;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Panel_BuyDetail;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_ItemIcon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ItemPrice;

	/** 구매 탭 보유 골드 표시(판매 탭 Text_CurrencyInInventory와 대응). */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_BuyCurrentGold;

	/** 구매 예정 금액(단가×수량) 표시. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_BuyTotalCost;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_OwnedCount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ItemName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_StatCurrent;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_StatNew;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ItemDesc;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_BuyDecrease;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_BuyQuantity;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_BuyIncrease;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Buy;

	// ── 판매 탭 — 카테고리 탭 버튼 ───────────────────────────────────────────
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_SellTab_Weapon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_SellTab_Consumable;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_SellTab_Material;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_SellTab_Armor;

	// ── 판매 탭 — 툴바 ───────────────────────────────────────────────────────
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCheckBox> CheckBox_SelectAll;

	/** "N개 선택 (N골드)" 표시 텍스트 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_SelectedCount;
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_SelectedCurrency;
	/** 선택한 판매 품목명과 판매/보유 수량을 한눈에 보여 주는 요약 영역. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_SellSelectionSummary;
	

	/** 선택 항목 일괄 판매 버튼 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_SellSelected;

	// ── 판매 수량 (단일 선택 시 부분 판매) ───────────────────────────────────
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_SellDecrease;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_SellIncrease;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_SellQuantity;

	/** 수량 조절 UI 묶음(단일 선택이 아닐 때 숨김). 없으면 개별 위젯만 토글. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> Panel_SellQuantity;

	/** 재구매 패널 열기 버튼 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_OpenRepurchase;

	// ── 판매 탭 — 그리드 / 재구매 전환 ──────────────────────────────────────
	/** 인덱스 0 = 아이템 그리드, 1 = 재구매 목록 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> Switcher_SellView;

	/** 아이템 그리드를 감싸는 ScrollBox */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> ScrollBox_SellGrid;

	/** 아이템 슬롯을 배치하는 UniformGridPanel (4열 고정, NativeConstruct에서 코드로 생성) */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UUniformGridPanel> UniformGrid_SellItems;

	// ── 판매 탭 — 재구매 뷰 ──────────────────────────────────────────────────
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_BackToSell;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> ScrollBox_RepurchaseList;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_RepurchaseTotalCost;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_RepurchaseAll;

private:
	// ── 버튼 바인딩 ────────────────────────────────────────────────────────────
	void BindButtonEvents();
	void ApplyShopTypeStyle();
	void ApplyShopModeLayout(bool bSellMode);
	void RefreshCurrencyText();

	// ── NPC 이름 / 대사 ───────────────────────────────────────────────────────
	void SetNpcNameText(const FText& InText);
	void SetNpcDialogueText(const FText& InText);
	FText GetShopDialogueText(FName DialogueRowName, const FText& FallbackText) const;
	void ShowShopDialogue(FName DialogueRowName, const FText& FallbackText);

	// ── 구매 탭 내부 ────────────────────────────────────────────────────────────
	void RefreshBuyList();
	void RefreshBuyDetail();
	/** 선택 품목의 구매 가능 최대 수량(무한 재고면 99, 유한이면 남은 재고(≤99)). */
	int32 GetBuyStockLimit() const;

	// ── 판매 탭 내부 ────────────────────────────────────────────────────────────
	void RefreshSellGrid(FGameplayTag CategoryTag);
	void UpdateSellSlotVisuals();
	void UpdateSellToolbar();
	void SelectRangeInSellGrid(int32 A, int32 B);
	void RefreshRepurchaseList();
	void PushRepurchaseRecord(FName ItemId, FGameplayTag Category, int32 Qty, int32 Price);
	void SyncRepurchaseHistoryToSave();
	void LoadRepurchaseHistoryFromSave();
	int32 GetSanitizedRepurchasePrice(FName ItemId, FGameplayTag CategoryTag, int32 Quantity, int32 StoredPrice) const;

	// ── 헬퍼 ────────────────────────────────────────────────────────────────────
	FText     GetItemDisplayName(FName ItemId, FGameplayTag CategoryTag) const;
	FText     GetItemShortDesc(FName ItemId, FGameplayTag CategoryTag) const;
	float     GetItemStatValue(FName ItemId, FGameplayTag CategoryTag) const;
	int32     GetItemBasePrice(FName ItemId, FGameplayTag CategoryTag) const;
	int32     CalcSellPrice(FName ItemId, FGameplayTag CategoryTag) const;
	// 아래 판매 슬롯 캐시 구조체(하단 정의)를 파라미터로 쓰는 헬퍼가 있어 전방 선언한다.
	struct FSellSlotCache;

	/** 해당 인벤토리 스택이 현재 장착 중인 장비인지(무기=장착 슬롯 ID, 방어구=장착 슬롯 목록). */
	bool      IsStackEquipped(const struct FRetrieveItemStack& Stack) const;
	/** 판매 슬롯용 상세 툴팁(WBP_ItemDetailTooltip)을 생성해 아이콘/등급/스탯/판매가/뱃지를 채운다. */
	UUserWidget* BuildSellSlotTooltip(const FSellSlotCache& SlotData) const;
	/** 카트에 슬롯을 담고(기본 수량=전량) 활성 슬롯으로 지정. 장착 슬롯은 무시. */
	void      AddSlotToCart(int32 SlotIndex, bool bMakeActive);
	UTexture2D* GetItemIcon(FName ItemId) const;

	// ── 버튼 핸들러 ─────────────────────────────────────────────────────────────
	UFUNCTION() void HandleCloseShopClicked();
	UFUNCTION() void HandleTabBuyClicked();
	UFUNCTION() void HandleTabSellClicked();
	UFUNCTION() void HandleBuyButtonClicked();
	UFUNCTION() void HandleBuyIncreaseClicked();
	UFUNCTION() void HandleBuyDecreaseClicked();
	UFUNCTION() void HandleSellTabWeaponClicked();
	UFUNCTION() void HandleSellTabConsumableClicked();
	UFUNCTION() void HandleSellTabMaterialClicked();
	UFUNCTION() void HandleSellTabArmorClicked();
	UFUNCTION() void HandleSellSelectedClicked();
	UFUNCTION() void HandleSellDecreaseClicked();
	UFUNCTION() void HandleSellIncreaseClicked();
	UFUNCTION() void HandleOpenRepurchaseClicked();
	UFUNCTION() void HandleBackToSellClicked();
	UFUNCTION() void HandleRepurchaseAllClicked();
	UFUNCTION() void HandleSelectAllChanged(bool bIsChecked);
	UFUNCTION() void HandleCurrencyChanged(int32 NewAmount);
	UFUNCTION() void HandleInventoryChanged();

	/** 판매 그리드 뷰 복귀: Switcher_SellView→0, Border_ListHeader/Overlay_85 Visible 복원 */
	void RestoreSellGridView();

	// ── 런타임 상태 ─────────────────────────────────────────────────────────────
	UPROPERTY() TObjectPtr<UInventoryComponent> InventoryComponent;
	UPROPERTY() TObjectPtr<URetrieveShopDefinitionAsset> ShopDefinition;
	UPROPERTY() TObjectPtr<URetrieveShopComponent> ShopComponentRef;

	bool bSellModeActive = false;

	// 판매 실행 중 재진입 가드. ExecuteSellSelected 루프 도중 RemoveItem이 유발하는
	// OnInventoryChanged → HandleInventoryChanged → RefreshSellGrid 재생성을 억제한다.
	bool bIsSelling = false;

	// 구매
	FName SelectedBuyRowName;
	int32 BuyQuantity     = 1;
	int32 SelectedBuyPrice = 0;

	// 판매 카트: 담긴 슬롯 인덱스 → 판매 예정 수량. 여러 종류를 각각 다른 수량으로 담아 한 번에 판매.
	TMap<int32, int32> SelectedSlotQuantities;
	// 수량 ± 조절 대상(가장 최근에 담거나 클릭한 슬롯). -1이면 없음.
	int32 ActiveSellSlotIndex = -1;

	// 판매 — 현재 카테고리 슬롯 캐시
	struct FSellSlotCache
	{
		FName        ItemId;
		FGameplayTag CategoryTag;
		int32        Quantity = 0;
		// 같은 아이템이 여러 슬롯에 나뉘어 있을 때 정확히 이 슬롯을 판매하기 위한 식별자.
		int32        SlotInstanceId = -1;
		// 이 슬롯이 현재 장착 중인 장비인지(판매 방지 + "장착" 배지 표시).
		bool         bEquipped = false;
	};
	TArray<FSellSlotCache>              CurrentSellSlots;
	TArray<TObjectPtr<UShopSellSlotWidget>> SellSlotWidgets;
	FGameplayTag                        CurrentSellCategoryTag;

	// 다중선택 상태
	TSet<int32> SelectedSlotIndices;
	int32       LastClickedSlotIndex  = -1;
	int32       DragStartSlotIndex    = -1;
	bool        bIsDragSelecting      = false;
	bool        bDragIsShiftDrag      = false;

	// 재구매 히스토리 (로컬 캐시, 판매/재구매 시 세이브 동기화)
	TArray<FShopRepurchaseRecord> RepurchaseHistory;
};
