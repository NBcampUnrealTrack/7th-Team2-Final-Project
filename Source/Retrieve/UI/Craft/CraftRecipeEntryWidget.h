#pragma once

#include "CoreMinimal.h"
#include "UI/VFX/RetrieveUIVFXWidget.h"
#include "CraftRecipeEntryWidget.generated.h"

class UButton;
class UCanvasPanel;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCraftRecipeEntryClickedSignature, FName, RecipeId);

UCLASS()
class RETRIEVE_API UCraftRecipeEntryWidget : public URetrieveUIVFXWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Craft")
	void InitRecipeEntry(FName InRecipeId, const FText& InDisplayName, int32 InMaxCraftableCount);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Retrieve|Craft")
	void SetSelected(bool bIsSelected);
	virtual void SetSelected_Implementation(bool bIsSelected);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Craft")
	void SetDescription(const FText& InDescription);

	UFUNCTION(BlueprintPure, Category = "Retrieve|Craft")
	FName GetRecipeId() const { return NativeRecipeId; }

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Craft|Events")
	FCraftRecipeEntryClickedSignature OnRecipeClicked;

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
	int32 MaxCraftableCount = 0;
	bool bSelected = false;
	bool bHovered = false;
};
