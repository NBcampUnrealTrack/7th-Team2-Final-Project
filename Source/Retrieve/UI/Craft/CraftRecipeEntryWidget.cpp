#include "UI/Craft/CraftRecipeEntryWidget.h"

#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"

void UCraftRecipeEntryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// WBP_RecipeEntry(Bonfire)는 Button_Select, 기존 WBP는 Button_Recipe
	UButton* ButtonToBind = Button_Recipe ? Button_Recipe : Button_Select;
	if (ButtonToBind)
	{
		ButtonToBind->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleClicked);

		// CanvasPanel 슬롯의 auto_size가 true이면 fill anchor가 무시되어
		// 버튼이 자식 크기로만 결정됨 → 전체 영역 클릭 불가. 런타임에 강제 해제.
		if (UCanvasPanelSlot* BtnSlot = Cast<UCanvasPanelSlot>(ButtonToBind->Slot))
		{
			BtnSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			BtnSlot->SetOffsets(FMargin(0.0f));
			BtnSlot->SetAlignment(FVector2D::ZeroVector);
			BtnSlot->SetAutoSize(false);
			BtnSlot->SynchronizeProperties();
		}
	}

	RefreshVisualState();
}

FReply UCraftRecipeEntryWidget::NativeOnPreviewMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		PlayUISound(ERetrieveUISoundEvent::Press);
		HandleClicked();
		return FReply::Handled();
	}

	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UCraftRecipeEntryWidget::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		HandleClicked();
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void UCraftRecipeEntryWidget::NativeOnMouseEnter(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseEnter(InGeometry, InMouseEvent);
	PlayUISound(ERetrieveUISoundEvent::Hover);
	bHovered = true;
	RefreshVisualState();
}

void UCraftRecipeEntryWidget::NativeOnMouseLeave(const FPointerEvent& InMouseEvent)
{
	Super::NativeOnMouseLeave(InMouseEvent);
	PlayUISound(ERetrieveUISoundEvent::Unhover);
	bHovered = false;
	RefreshVisualState();
}

void UCraftRecipeEntryWidget::InitRecipeEntry(FName InRecipeId, const FText& InDisplayName, int32 InOwnedCount)
{
	NativeRecipeId = InRecipeId;
	DisplayName = InDisplayName;
	OwnedCount = FMath::Max(0, InOwnedCount);
	RefreshVisualState();
}

void UCraftRecipeEntryWidget::SetSelected_Implementation(bool bIsSelected)
{
	bSelected = bIsSelected;
	RefreshVisualState();
}

void UCraftRecipeEntryWidget::SetDescription(const FText& InDescription)
{
	if (Text_RecipeDescription)
	{
		Text_RecipeDescription->SetText(InDescription);
	}
}

void UCraftRecipeEntryWidget::SetIconTexture(UTexture2D* InTexture)
{
	if (Image_RecipeIcon && InTexture)
	{
		Image_RecipeIcon->SetBrushFromTexture(InTexture, true);
		Image_RecipeIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UCraftRecipeEntryWidget::HandleClicked()
{
	OnRecipeClicked.Broadcast(NativeRecipeId);
}

void UCraftRecipeEntryWidget::RefreshVisualState()
{
	if (Text_RecipeName)
	{
		Text_RecipeName->SetText(DisplayName.IsEmpty() ? FText::FromName(NativeRecipeId) : DisplayName);
		Text_RecipeName->SetColorAndOpacity(FSlateColor(
			bSelected ? RecipeNameSelectedColor : bHovered ? RecipeNameHoveredColor : RecipeNameNormalColor));
	}

	if (Text_CraftableCount)
	{
		Text_CraftableCount->SetText(FText::Format(
			NSLOCTEXT("CraftRecipeEntry", "OwnedCount", "보유: {0}"),
			FText::AsNumber(OwnedCount)));
		Text_CraftableCount->SetColorAndOpacity(FSlateColor(
			OwnedCount > 0
				? FLinearColor(0.76f, 0.92f, 0.64f, 1.0f)
				: FLinearColor(0.6f, 0.6f, 0.6f, 1.0f)));
	}

	if (Arrow)
	{
		Arrow->SetVisibility(bSelected ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Hidden);
	}

	if (Image_SelectGlow)
	{
		Image_SelectGlow->SetVisibility(
			bSelected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden);
	}
}
