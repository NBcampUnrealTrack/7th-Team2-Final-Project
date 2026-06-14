#include "UI/Craft/CraftMaterialRowWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"

const FLinearColor UCraftMaterialRowWidget::SufficientColor = FLinearColor(0.9f, 0.9f, 0.9f, 1.f);
const FLinearColor UCraftMaterialRowWidget::InsufficientColor = FLinearColor(0.9f, 0.1f, 0.1f, 1.f);

void UCraftMaterialRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UCraftMaterialRowWidget::InitMaterialRow(
	UDataTable* InIconTable,
	UDataTable* InMatTable,
	FName InItemId,
	int32 InRequired,
	int32 InOwned)
{
	IconTable = InIconTable;
	MatTable = InMatTable;
	NativeItemId = InItemId;
	RequiredCount = FMath::Max(1, InRequired);
	OwnedCount = FMath::Max(0, InOwned);

	if (Image_MatIcon && IconTable)
	{
		if (const FRetrieveItemIconRow* IconRow =
			IconTable->FindRow<FRetrieveItemIconRow>(NativeItemId, TEXT("CraftMaterialRow")))
		{
			if (UTexture2D* Icon = IconRow->IconTexture.LoadSynchronous())
			{
				Image_MatIcon->SetBrushFromTexture(Icon, true);
			}
		}
	}

	FText MaterialName = FText::FromName(NativeItemId);
	if (Text_MatName && MatTable)
	{
		if (const FRetrieveMaterialItemRow* MatRow =
			MatTable->FindRow<FRetrieveMaterialItemRow>(NativeItemId, TEXT("CraftMaterialRow")))
		{
			MaterialName = MatRow->DisplayName;
		}
	}
	if (Text_MatName)
	{
		Text_MatName->SetText(MaterialName);
		Text_MatName->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	ApplyCountColor();
}

void UCraftMaterialRowWidget::RefreshOwnedCount(int32 InOwned)
{
	OwnedCount = FMath::Max(0, InOwned);
	ApplyCountColor();
}

void UCraftMaterialRowWidget::ApplyCountColor()
{
	if (!Text_MatCount)
	{
		return;
	}

	Text_MatCount->SetText(FText::Format(
		NSLOCTEXT("CraftMaterialRow", "RequiredCount", "x{0}"),
		FText::AsNumber(RequiredCount)));
	Text_MatCount->SetToolTipText(FText::Format(
		NSLOCTEXT("CraftMaterialRow", "OwnedRequiredTooltip", "Owned {0} / Required {1}"),
		FText::AsNumber(OwnedCount),
		FText::AsNumber(RequiredCount)));

	Text_MatCount->SetColorAndOpacity(FSlateColor(
		IsSufficient() ? SufficientColor : InsufficientColor));
}
