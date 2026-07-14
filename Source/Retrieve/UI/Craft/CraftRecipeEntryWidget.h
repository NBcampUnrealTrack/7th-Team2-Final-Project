#pragma once

#include "CoreMinimal.h"
#include "UI/VFX/RetrieveUIVFXWidget.h"
#include "CraftRecipeEntryWidget.generated.h"

class UButton;
class UCanvasPanel;
class UImage;
class UTextBlock;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCraftRecipeEntryClickedSignature, FName, RecipeId);

UCLASS()
class RETRIEVE_API UCraftRecipeEntryWidget : public URetrieveUIVFXWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Craft")
	void InitRecipeEntry(FName InRecipeId, const FText& InDisplayName, int32 InOwnedCount);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Retrieve|Craft")
	void SetSelected(bool bIsSelected);
	virtual void SetSelected_Implementation(bool bIsSelected);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Craft")
	void SetDescription(const FText& InDescription);

	/** 결과물 아이콘 텍스처 설정 (CraftPanel이 ItemIconTable에서 조회해 주입) */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Craft")
	void SetIconTexture(UTexture2D* InTexture);

	UFUNCTION(BlueprintPure, Category = "Retrieve|Craft")
	FName GetRecipeId() const { return NativeRecipeId; }

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Craft|Events")
	FCraftRecipeEntryClickedSignature OnRecipeClicked;

	// ── 레시피 이름 글자색 ────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Craft|Colors")
	FLinearColor RecipeNameSelectedColor = FLinearColor(1.0f, 0.84f, 0.35f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Craft|Colors")
	FLinearColor RecipeNameHoveredColor = FLinearColor(1.0f, 0.95f, 0.78f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Craft|Colors")
	FLinearColor RecipeNameNormalColor = FLinearColor(0.95f, 0.88f, 0.72f, 1.0f);
	
protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnPreviewMouseButtonDown(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseEnter(
		const FGeometry& InGeometry,
		const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseLeave(const FPointerEvent& InMouseEvent) override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Recipe;

	// WBP_RecipeEntry(Bonfire)에서는 버튼 이름이 Button_Select
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Select;

	// 결과물 아이콘 (WBP_RecipeEntry에 추가)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_RecipeIcon;

	// 선택 시 표시되는 청색 글로우 (WBP_RecipeEntry에 추가)
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_SelectGlow;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_RecipeName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CraftableCount;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_RecipeDescription;

	// WBP 내 CanvasPanel "Arrow" — 선택 중인 레시피에만 표시
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> Arrow;

private:
	UFUNCTION()
	void HandleClicked();

	void RefreshVisualState();

	FName NativeRecipeId;
	FText DisplayName;
	int32 OwnedCount = 0;
	bool bSelected = false;
	bool bHovered = false;
};
