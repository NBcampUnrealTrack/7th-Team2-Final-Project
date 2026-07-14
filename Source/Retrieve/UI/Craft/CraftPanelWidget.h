#pragma once

#include "CoreMinimal.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameplayTagContainer.h"
#include "UI/RetrieveGamePanelWidget.h"
#include "CraftPanelWidget.generated.h"

class UInventoryComponent;
class UCraftMaterialRowWidget;
class UCraftRecipeEntryWidget;
class UScrollBox;
class UTextBlock;
class UImage;
class UButton;
class USlider;
class USizeBox;
class UVerticalBox;
class UDataTable;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCraftPanelCraftedSignature, FName, RecipeId, bool, bSuccess);

/** 제작 카테고리 사이드바 항목 */
UENUM(BlueprintType)
enum class ECraftCategory : uint8
{
	Consumable	UMETA(DisplayName = "소모품"),
	Buff		UMETA(DisplayName = "버프 아이템"),
	Material	UMETA(DisplayName = "강화 재료"),
	Equipment	UMETA(DisplayName = "장비"),
	Etc			UMETA(DisplayName = "기타")
};

/**
 * 제작 UI 메인 패널 (3컬럼 레이아웃).
 *
 * ─ 레이아웃 (WBP에서 구성) ─────────────────────────────────────────────────
 *   HorizontalBox_CraftBody
 *     ① VerticalBox_Categories      — 카테고리 사이드바 (Button_Cat_*)
 *     ② ScrollBox_RecipeList        — 레시피 목록 (선택 카테고리로 필터)
 *     ③ VerticalBox_CraftRoot       — 선택 레시피 상세 (고정 우측 컬럼, 상시 표시)
 *         Image_OutputIcon          — 결과물 아이콘
 *         Text_OutputName           — 결과물 이름
 *         Text_MaxCraftable         — "보유: N"
 *         Text_OutputDescription    — 설명
 *         VerticalBox_Materials     — C++이 재료 행을 동적 생성
 *         (Slider_CraftCount)       — 선택적 슬라이더
 *         Button_CountMinus/Plus/Max + Text_CraftCount — 수량 스테퍼
 *         Button_Craft              — 제작 실행
 *
 * ─ 사용 방법 ──────────────────────────────────────────────────────────────
 * 1. BP WBP_CraftPanel을 이 클래스 기반으로 만든다.
 * 2. PlayerController 또는 HUD에서 InitializeCraftPanel() 호출.
 * 3. 레시피 목록 셀(WBP_RecipeEntry)에서 클릭 시 SelectRecipe(RecipeId) 호출.
 * 4. Button_Craft 클릭 → ExecuteCraft() 자동 호출됨 (BindButtonEvents에서 바인딩).
 */
UCLASS()
class RETRIEVE_API UCraftPanelWidget : public URetrieveGamePanelWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Craft")
	void InitializeCraftPanel(UInventoryComponent* InInventoryComponent);

	/** 레시피 목록 셀 클릭 시 호출. 상세 패널을 갱신한다 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Craft")
	void SelectRecipe(FName InRecipeId);

	/** 카테고리 사이드바 선택 (BP에서도 호출 가능) */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Craft")
	void SetActiveCategory(ECraftCategory InCategory);

	/** Button_Craft에 자동 바인딩됨. 직접 호출 가능 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Craft")
	bool ExecuteCraft();

	UFUNCTION(BlueprintPure, Category = "Retrieve|Craft")
	FName GetSelectedRecipeId() const { return SelectedRecipeId; }

	UFUNCTION(BlueprintPure, Category = "Retrieve|Craft")
	int32 GetCraftCount() const { return CraftCount; }

	UFUNCTION(BlueprintPure, Category = "Retrieve|Craft")
	bool CanExecuteCraft() const;

	/** 레시피 목록 전체 갱신 요청 — BP가 OnInventoryChanged에서 구독 */
	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Craft|Events")
	FCraftPanelCraftedSignature OnCrafted;

	// ── DataTable 참조 ──────────────────────────────────────────────────────
	/** FRetrieveCraftRecipeRow rows */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Craft|Data")
	TObjectPtr<UDataTable> CraftRecipeTable;

	/** FRetrieveItemIconRow rows — 결과물·재료 아이콘 조회 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Craft|Data")
	TObjectPtr<UDataTable> ItemIconTable;

	/** FRetrieveMaterialItemRow rows — 재료 이름 조회 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Craft|Data")
	TObjectPtr<UDataTable> MaterialItemTable;

	/** FRetrieveConsumableItemRow rows — 결과물 설명 조회 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Craft|Data")
	TObjectPtr<UDataTable> ConsumableItemTable;

	/** FRetrieveWeaponDataRow rows — 결과물 설명 조회 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Craft|Data")
	TObjectPtr<UDataTable> WeaponDataTable;

	/** 재료 행 위젯 클래스 (WBP_CraftMaterialRow) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Craft|Data")
	TSubclassOf<UCraftMaterialRowWidget> MaterialRowWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Craft|Data")
	TSubclassOf<UCraftRecipeEntryWidget> RecipeEntryWidgetClass;

	/** Limits the recipe scroll viewport when the Blueprint parent gives it too much Fill space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Craft|Layout")
	float RecipeListMaxHeight = 0.0f;

	// ── 카테고리 필터 태그 (디자이너 조정 가능) ──────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Craft|Category", meta = (Categories = "Item"))
	FGameplayTag Tag_Consumable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Craft|Category", meta = (Categories = "Item"))
	FGameplayTag Tag_Buff;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Craft|Category", meta = (Categories = "Item"))
	FGameplayTag Tag_Material;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Craft|Category", meta = (Categories = "Item"))
	FGameplayTagContainer Tags_Equipment;
	
	/** 선택된 카테고리 아이콘 틴트 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Craft|Category")
	FLinearColor CategoryIconSelectedColor = FLinearColor(1.0f, 0.82f, 0.25f, 1.0f);

	/** 비선택 카테고리 아이콘 틴트 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Craft|Category")
	FLinearColor CategoryIconNormalColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

protected:
	virtual void NativeConstruct() override;

	// ── 위젯 바인딩 ────────────────────────────────────────────────────────
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> ScrollBox_RecipeList;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> VerticalBox_CraftRoot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UVerticalBox> VerticalBox_Materials;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_OutputIcon;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_OutputName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_MaxCraftable;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_OutputDescription;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<USlider> Slider_CraftCount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CraftCount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CraftCountLabel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CraftButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Craft;

	// ── 수량 스테퍼 ─────────────────────────────────────────────────────────
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_CountMinus;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_CountPlus;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_CountMax;

	// ── 카테고리 사이드바 버튼 ──────────────────────────────────────────────
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Cat_Consumable;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Cat_Buff;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Cat_Material;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Cat_Equip;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Cat_Etc;
	
	// ── 카테고리 사이드바 아이콘 ────────────────────────────────────────────
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Btn_Consumable;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Btn_Buff;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Btn_Material;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Btn_Equip;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Img_Btn_Etc;

private:
	void BindButtonEvents();
	void ApplyStaticTexts();
	void ResolveDefaultDataAssets();
	void RefreshRecipeList();
	void AddRecipeEntry(FName RecipeId, const FRetrieveCraftRecipeRow& Recipe);
	void AddRecipeEntryToList(UCraftRecipeEntryWidget* RecipeEntry);
	void RefreshRecipeEntrySelection();
	void RefreshDetailPanel();
	void RefreshMaterialRows(const FRetrieveCraftRecipeRow& Recipe);
	void ClearMaterialRows();
	void UpdateCraftCountUI();
	void SetCraftCount(int32 NewCount);
	bool PassesCategoryFilter(const FRetrieveCraftRecipeRow& Recipe) const;
	void SelectFirstVisibleRecipe();
	void RefreshCategoryButtons();
	void ApplyCraftButtonEnabledStyle(bool bEnabled);
	void BeginTimedCraft();
	void HandleTimedCraftComplete();

	UFUNCTION()
	void HandleCraftButtonClicked();

	UFUNCTION()
	void HandleSliderValueChanged(float Value);

	UFUNCTION()
	void HandleCountMinusClicked();

	UFUNCTION()
	void HandleCountPlusClicked();

	UFUNCTION()
	void HandleCountMaxClicked();

	UFUNCTION()
	void HandleCatConsumableClicked();

	UFUNCTION()
	void HandleCatBuffClicked();

	UFUNCTION()
	void HandleCatMaterialClicked();

	UFUNCTION()
	void HandleCatEquipClicked();

	UFUNCTION()
	void HandleCatEtcClicked();

	UFUNCTION()
	void HandleInventoryChanged();

	UFUNCTION()
	void HandleCraftCompleted(bool bSuccess, FName RecipeId, FName OutputItemId);

	UFUNCTION()
	void HandleRecipeEntryClicked(FName RecipeId);

	UPROPERTY()
	TObjectPtr<UInventoryComponent> InventoryComponent;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UCraftRecipeEntryWidget>> RecipeEntries;

	UPROPERTY(Transient)
	TArray<FName> RecipeListOrder;

	ECraftCategory ActiveCategory = ECraftCategory::Consumable;
	FName SelectedRecipeId;
	int32 CraftCount = 1;
	int32 MaxCraftableCount = 0;
	bool bSelectedRecipeIsEnhancement = false;

	// ── 타임드 제작 진행 상태 ───────────────────────────────────────────────
	bool bIsCrafting = false;
	FName PendingRecipeId;
	int32 PendingCraftCount = 0;
	ECraftCategory PendingCraftCategory = ECraftCategory::Consumable;

	static const FLinearColor CraftButtonEnabledColor;
	static const FLinearColor CraftButtonDisabledColor;
};
