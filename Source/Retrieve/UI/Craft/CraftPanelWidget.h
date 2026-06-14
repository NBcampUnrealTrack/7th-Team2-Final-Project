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

/**
 * 제작 UI 메인 패널.
 *
 * ─ 레이아웃 (WBP에서 구성) ─────────────────────────────────────────────────
 *   ScrollBox_RecipeList       — 레시피 목록 (BP에서 항목 생성 후 SelectRecipe 호출)
 *   VerticalBox_RecipeDetail   — 선택 레시피 상세
 *     Image_OutputIcon         — 결과물 아이콘
 *     Text_OutputName          — 결과물 이름
 *     Text_MaxCraftable        — "최대 N개 제작 가능"
 *     Text_OutputDescription   — 설명
 *     VerticalBox_Materials    — C++이 재료 행을 동적 생성
 *     Slider_CraftCount        — 제작 수량 (1~MaxCraftable)
 *     Text_CraftCount          — 슬라이더 값 표시
 *     Button_Craft             — 제작 실행
 *
 * ─ 사용 방법 ──────────────────────────────────────────────────────────────
 * 1. BP WBP_CraftPanel을 이 클래스 기반으로 만든다.
 * 2. PlayerController 또는 HUD에서 InitializeCraftPanel() 호출.
 * 3. 레시피 목록 셀(WBP_CraftRecipeEntry)에서 클릭 시 SelectRecipe(RecipeId) 호출.
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
	float RecipeListMaxHeight = 360.0f;

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

private:
	void BindButtonEvents();
	void ApplyStaticTexts();
	void ResolveDefaultDataAssets();
	void WrapRecipeListInBounds();
	void RefreshRecipeList();
	void AddRecipeEntry(FName RecipeId, const FRetrieveCraftRecipeRow& Recipe);
	void AddRecipeEntryToList(UCraftRecipeEntryWidget* RecipeEntry);
	void AddDetailToList();
	void RebuildRecipeListLayout();
	void RefreshRecipeEntrySelection();
	void AttachDetailToSelectedRecipe();
	void CollapseSelectedRecipe();
	void RefreshDetailPanel();
	void RefreshMaterialRows(const FRetrieveCraftRecipeRow& Recipe);
	void ClearMaterialRows();
	void UpdateCraftCountUI();

	UFUNCTION()
	void HandleCraftButtonClicked();

	UFUNCTION()
	void HandleSliderValueChanged(float Value);

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

	FName SelectedRecipeId;
	int32 CraftCount = 1;
	int32 MaxCraftableCount = 0;
};
