#include "UI/Craft/CraftMaterialRowWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"

const FLinearColor UCraftMaterialRowWidget::SufficientColor = FLinearColor(0.49f, 0.83f, 0.40f, 1.f);
const FLinearColor UCraftMaterialRowWidget::InsufficientColor = FLinearColor(0.88f, 0.30f, 0.26f, 1.f);

void UCraftMaterialRowWidget::NativeConstruct()
{
	Super::NativeConstruct();
}

void UCraftMaterialRowWidget::InitMaterialRow(
	UDataTable* InIconTable,
	UDataTable* InMatTable,
	FName InItemId,
	int32 InRequired,
	int32 InOwned,
	UDataTable* InWeaponTable)
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
	if (const FRetrieveMaterialItemRow* MatRow = MatTable
		? MatTable->FindRow<FRetrieveMaterialItemRow>(NativeItemId, TEXT("CraftMaterialRow"), false)
		: nullptr)
	{
		MaterialName = MatRow->DisplayName;
	}
	else if (const FRetrieveWeaponDataRow* WeaponRow = InWeaponTable
		? InWeaponTable->FindRow<FRetrieveWeaponDataRow>(NativeItemId, TEXT("CraftMaterialRow"), false)
		: nullptr)
	{
		MaterialName = WeaponRow->DisplayName;
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
		NSLOCTEXT("CraftMaterialRow", "OwnedRequired", "{0} / {1}"),
		FText::AsNumber(OwnedCount),
		FText::AsNumber(RequiredCount)));
	Text_MatCount->SetToolTipText(FText::Format(
		NSLOCTEXT("CraftMaterialRow", "OwnedRequiredTooltip", "Owned {0} / Required {1}"),
		FText::AsNumber(OwnedCount),
		FText::AsNumber(RequiredCount)));

	Text_MatCount->SetColorAndOpacity(FSlateColor(
		IsSufficient() ? SufficientColor : InsufficientColor));
}
