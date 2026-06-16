#include "UI/Craft/CraftPanelWidget.h"

#include "UI/Craft/CraftMaterialRowWidget.h"
#include "UI/Craft/CraftRecipeEntryWidget.h"
#include "UI/RetrieveItemDescriptionHelper.h"
#include "Components/Inventory/InventoryComponent.h"
#include "Data/RetrieveDataTableTypes.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/Slider.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"

void UCraftPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ResolveDefaultDataAssets();
	ApplyStaticTexts();
	BindButtonEvents();
	WrapRecipeListInBounds();
	if (ScrollBox_RecipeList)
	{
		ScrollBox_RecipeList->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	}
	RefreshRecipeList();
}

void UCraftPanelWidget::InitializeCraftPanel(UInventoryComponent* InInventoryComponent)
{
	if (!InInventoryComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("CraftPanelWidget: InitializeCraftPanel called with null InventoryComponent"));
		return;
	}

	InventoryComponent = InInventoryComponent;

	InventoryComponent->OnInventoryChanged.AddUniqueDynamic(this, &UCraftPanelWidget::HandleInventoryChanged);
	InventoryComponent->OnCraftCompleted.AddUniqueDynamic(this, &ThisClass::HandleCraftCompleted);

	if (!CraftRecipeTable && InventoryComponent->CraftRecipeTable)
	{
		CraftRecipeTable = InventoryComponent->CraftRecipeTable;
	}

	ResolveDefaultDataAssets();
	RefreshRecipeList();
}

void UCraftPanelWidget::SelectRecipe(FName InRecipeId)
{
	if (!CraftRecipeTable || InRecipeId.IsNone())
	{
		return;
	}

	if (SelectedRecipeId == InRecipeId
		&& VerticalBox_CraftRoot
		&& VerticalBox_CraftRoot->GetVisibility() != ESlateVisibility::Collapsed)
	{
		CollapseSelectedRecipe();
		return;
	}

	const FRetrieveCraftRecipeRow* Recipe =
		CraftRecipeTable->FindRow<FRetrieveCraftRecipeRow>(InRecipeId, TEXT("CraftPanel::SelectRecipe"));
	if (!Recipe)
	{
		UE_LOG(LogTemp, Warning, TEXT("CraftPanelWidget: Recipe '%s' not found"), *InRecipeId.ToString());
		return;
	}

	SelectedRecipeId = InRecipeId;
	MaxCraftableCount = InventoryComponent ? InventoryComponent->GetMaxCraftableCount(InRecipeId) : 0;
	CraftCount = FMath::Clamp(CraftCount, 1, FMath::Max(1, MaxCraftableCount));

	RefreshDetailPanel();
	AttachDetailToSelectedRecipe();
	RefreshRecipeEntrySelection();
}

bool UCraftPanelWidget::CanExecuteCraft() const
{
	return InventoryComponent
		&& !SelectedRecipeId.IsNone()
		&& MaxCraftableCount >= CraftCount
		&& CraftCount >= 1;
}

bool UCraftPanelWidget::ExecuteCraft()
{
	if (!CanExecuteCraft())
	{
		return false;
	}

	bool bAllSuccess = true;
	for (int32 i = 0; i < CraftCount; ++i)
	{
		if (!InventoryComponent->CraftItem(SelectedRecipeId))
		{
			bAllSuccess = false;
			break;
		}
	}

	return bAllSuccess;
}

void UCraftPanelWidget::BindButtonEvents()
{
	if (Button_Craft && !Button_Craft->OnClicked.IsBound())
	{
		Button_Craft->OnClicked.AddDynamic(this, &UCraftPanelWidget::HandleCraftButtonClicked);
	}

	if (Slider_CraftCount && !Slider_CraftCount->OnValueChanged.IsBound())
	{
		Slider_CraftCount->OnValueChanged.AddDynamic(this, &UCraftPanelWidget::HandleSliderValueChanged);
		Slider_CraftCount->SetMinValue(1.f);
		Slider_CraftCount->SetMaxValue(1.f);
		Slider_CraftCount->SetValue(1.f);
		Slider_CraftCount->SetStepSize(1.f);
	}
}

void UCraftPanelWidget::ApplyStaticTexts()
{
	if (Text_CraftCountLabel)
	{
		Text_CraftCountLabel->SetText(NSLOCTEXT("CraftPanel", "CraftCountLabel", "Count:"));
	}

	if (Text_CraftButton)
	{
		Text_CraftButton->SetText(NSLOCTEXT("CraftPanel", "CraftButton", "Craft"));
	}
}

void UCraftPanelWidget::ResolveDefaultDataAssets()
{
	if (!CraftRecipeTable)
	{
		CraftRecipeTable = LoadObject<UDataTable>(
			nullptr,
			TEXT("/Game/Retrieve/Data/Items/DT_CraftRecipe.DT_CraftRecipe"));
	}
	if (!ItemIconTable)
	{
		ItemIconTable = LoadObject<UDataTable>(
			nullptr,
			TEXT("/Game/Retrieve/Data/Items/DT_ItemIcon.DT_ItemIcon"));
	}
	if (!MaterialItemTable)
	{
		MaterialItemTable = LoadObject<UDataTable>(
			nullptr,
			TEXT("/Game/Retrieve/Data/Items/DT_MaterialItem.DT_MaterialItem"));
	}
	if (!MaterialRowWidgetClass)
	{
		MaterialRowWidgetClass = LoadClass<UCraftMaterialRowWidget>(
			nullptr,
			TEXT("/Game/Retrieve/UI/Craft/WBP_CraftMaterialRow.WBP_CraftMaterialRow_C"));
	}
	RecipeEntryWidgetClass = LoadClass<UCraftRecipeEntryWidget>(
		nullptr,
		TEXT("/Game/Retrieve/UI/Bonfire/WBP_RecipeEntry.WBP_RecipeEntry_C"));
	if (!ConsumableItemTable)
	{
		ConsumableItemTable = LoadObject<UDataTable>(
			nullptr,
			TEXT("/Game/Retrieve/Data/Items/DT_ConsumableItem.DT_ConsumableItem"));
	}
	if (!WeaponDataTable)
	{
		WeaponDataTable = LoadObject<UDataTable>(
			nullptr,
			TEXT("/Game/Retrieve/Data/Items/DT_WeaponData.DT_WeaponData"));
	}
}

void UCraftPanelWidget::WrapRecipeListInBounds()
{
	if (!ScrollBox_RecipeList || RecipeListMaxHeight <= 0.0f)
	{
		return;
	}

	if (Cast<USizeBox>(ScrollBox_RecipeList->GetParent()))
	{
		return;
	}

	UVerticalBox* ParentVerticalBox = Cast<UVerticalBox>(ScrollBox_RecipeList->GetParent());
	if (!ParentVerticalBox)
	{
		return;
	}

	const int32 ChildIndex = ParentVerticalBox->GetChildIndex(ScrollBox_RecipeList);
	if (ChildIndex == INDEX_NONE)
	{
		return;
	}

	FMargin OriginalPadding = FMargin(0.0f);
	EHorizontalAlignment OriginalHorizontalAlignment = HAlign_Fill;
	EVerticalAlignment OriginalVerticalAlignment = VAlign_Fill;

	if (UVerticalBoxSlot* OriginalSlot = Cast<UVerticalBoxSlot>(ScrollBox_RecipeList->Slot))
	{
		OriginalPadding = OriginalSlot->GetPadding();
		OriginalHorizontalAlignment = OriginalSlot->GetHorizontalAlignment();
		OriginalVerticalAlignment = OriginalSlot->GetVerticalAlignment();
	}

	USizeBox* RecipeListBounds = NewObject<USizeBox>(this, TEXT("SizeBox_RecipeListBounds"));
	if (!RecipeListBounds)
	{
		return;
	}

	RecipeListBounds->SetMaxDesiredHeight(RecipeListMaxHeight);
	RecipeListBounds->SetClipping(EWidgetClipping::ClipToBoundsAlways);

	ScrollBox_RecipeList->RemoveFromParent();

	if (UVerticalBoxSlot* BoundsSlot = Cast<UVerticalBoxSlot>(
		ParentVerticalBox->InsertChildAt(ChildIndex, RecipeListBounds)))
	{
		BoundsSlot->SetPadding(OriginalPadding);
		BoundsSlot->SetHorizontalAlignment(OriginalHorizontalAlignment);
		BoundsSlot->SetVerticalAlignment(OriginalVerticalAlignment);
		BoundsSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	}

	RecipeListBounds->AddChild(ScrollBox_RecipeList);
}

void UCraftPanelWidget::RefreshRecipeList()
{
	if (!ScrollBox_RecipeList || !CraftRecipeTable)
	{
		return;
	}

	// CraftRoot가 ScrollBox 자식으로 이동된 경우 ClearChildren에 지워지므로 먼저 분리
	const bool bCraftRootInScrollBox = VerticalBox_CraftRoot
		&& VerticalBox_CraftRoot->GetParent() == ScrollBox_RecipeList;
	if (bCraftRootInScrollBox)
	{
		VerticalBox_CraftRoot->RemoveFromParent();
	}
	if (VerticalBox_CraftRoot)
	{
		VerticalBox_CraftRoot->SetVisibility(ESlateVisibility::Collapsed);
	}

	ScrollBox_RecipeList->ClearChildren();
	RecipeEntries.Reset();
	RecipeListOrder.Reset();

	const TArray<FName> RowNames = CraftRecipeTable->GetRowNames();
	for (const FName& RecipeId : RowNames)
	{
		if (const FRetrieveCraftRecipeRow* Recipe =
			CraftRecipeTable->FindRow<FRetrieveCraftRecipeRow>(RecipeId, TEXT("CraftPanel::RefreshRecipeList"), false))
		{
			AddRecipeEntry(RecipeId, *Recipe);
		}
	}

	if (!SelectedRecipeId.IsNone()
		&& CraftRecipeTable->FindRow<FRetrieveCraftRecipeRow>(SelectedRecipeId, TEXT("CraftPanel::RefreshRecipeListSelected"), false))
	{
		MaxCraftableCount = InventoryComponent ? InventoryComponent->GetMaxCraftableCount(SelectedRecipeId) : 0;
		CraftCount = FMath::Clamp(CraftCount, 1, FMath::Max(1, MaxCraftableCount));
		RefreshDetailPanel();
		AttachDetailToSelectedRecipe();
	}
}

void UCraftPanelWidget::AddRecipeEntry(FName RecipeId, const FRetrieveCraftRecipeRow& Recipe)
{
	if (!RecipeEntryWidgetClass)
	{
		return;
	}

	UCraftRecipeEntryWidget* RecipeEntry = CreateWidget<UCraftRecipeEntryWidget>(
		GetOwningPlayer(), RecipeEntryWidgetClass);
	if (!RecipeEntry)
	{
		return;
	}

	const int32 MaxCount = InventoryComponent
		? InventoryComponent->GetMaxCraftableCount(RecipeId)
		: 0;
	RecipeEntry->InitRecipeEntry(
		RecipeId,
		Recipe.DisplayName.IsEmpty() ? FText::FromName(RecipeId) : Recipe.DisplayName,
		MaxCount);
	RecipeEntry->SetDescription(URetrieveItemDescriptionHelper::BuildItemDescription(
		Recipe.OutputItem.ItemId, Recipe.OutputItem.ItemCategoryTag,
		ConsumableItemTable, MaterialItemTable, WeaponDataTable));
	RecipeEntry->OnRecipeClicked.AddUniqueDynamic(this, &ThisClass::HandleRecipeEntryClicked);

	RecipeEntries.Add(RecipeId, RecipeEntry);
	RecipeListOrder.Add(RecipeId);
	AddRecipeEntryToList(RecipeEntry);
	RefreshRecipeEntrySelection();
}

void UCraftPanelWidget::AddRecipeEntryToList(UCraftRecipeEntryWidget* RecipeEntry)
{
	if (!ScrollBox_RecipeList || !RecipeEntry)
	{
		return;
	}

	if (UScrollBoxSlot* EntrySlot = Cast<UScrollBoxSlot>(ScrollBox_RecipeList->AddChild(RecipeEntry)))
	{
		EntrySlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		EntrySlot->SetHorizontalAlignment(HAlign_Fill);
		EntrySlot->SetVerticalAlignment(VAlign_Top);
	}
}

void UCraftPanelWidget::AddDetailToList()
{
	if (!ScrollBox_RecipeList || !VerticalBox_CraftRoot)
	{
		return;
	}

	if (UScrollBoxSlot* DetailSlot = Cast<UScrollBoxSlot>(ScrollBox_RecipeList->AddChild(VerticalBox_CraftRoot)))
	{
		DetailSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 4.0f));
		DetailSlot->SetHorizontalAlignment(HAlign_Fill);
		DetailSlot->SetVerticalAlignment(VAlign_Top);
	}
}

void UCraftPanelWidget::RebuildRecipeListLayout()
{
	if (!ScrollBox_RecipeList)
	{
		return;
	}

	if (VerticalBox_CraftRoot && VerticalBox_CraftRoot->GetParent())
	{
		VerticalBox_CraftRoot->RemoveFromParent();
	}

	ScrollBox_RecipeList->ClearChildren();

	for (const FName& RecipeId : RecipeListOrder)
	{
		UCraftRecipeEntryWidget* Entry = RecipeEntries.FindRef(RecipeId);
		AddRecipeEntryToList(Entry);

		if (RecipeId == SelectedRecipeId && VerticalBox_CraftRoot)
		{
			AddDetailToList();
		}
	}
}

void UCraftPanelWidget::RefreshRecipeEntrySelection()
{
	for (const TPair<FName, TObjectPtr<UCraftRecipeEntryWidget>>& Pair : RecipeEntries)
	{
		if (UCraftRecipeEntryWidget* Entry = Pair.Value.Get())
		{
			Entry->SetSelected(Pair.Key == SelectedRecipeId);
		}
	}
}

void UCraftPanelWidget::AttachDetailToSelectedRecipe()
{
	if (!VerticalBox_CraftRoot || !ScrollBox_RecipeList)
	{
		return;
	}

	VerticalBox_CraftRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	RebuildRecipeListLayout();
}

void UCraftPanelWidget::CollapseSelectedRecipe()
{
	SelectedRecipeId = NAME_None;
	MaxCraftableCount = 0;
	CraftCount = 1;
	ClearMaterialRows();

	if (VerticalBox_CraftRoot)
	{
		VerticalBox_CraftRoot->SetVisibility(ESlateVisibility::Collapsed);
	}

	UpdateCraftCountUI();
	RefreshRecipeEntrySelection();
}

void UCraftPanelWidget::RefreshDetailPanel()
{
	if (SelectedRecipeId.IsNone() || !CraftRecipeTable)
	{
		return;
	}

	const FRetrieveCraftRecipeRow* Recipe =
		CraftRecipeTable->FindRow<FRetrieveCraftRecipeRow>(SelectedRecipeId, TEXT("CraftPanel::Refresh"));
	if (!Recipe)
	{
		return;
	}

	if (Image_OutputIcon && ItemIconTable)
	{
		if (const FRetrieveItemIconRow* IconRow =
			ItemIconTable->FindRow<FRetrieveItemIconRow>(Recipe->OutputItem.ItemId, TEXT("CraftPanel::Icon")))
		{
			if (UTexture2D* Tex = IconRow->IconTexture.LoadSynchronous())
			{
				Image_OutputIcon->SetBrushFromTexture(Tex, true);
			}
		}
	}

	if (Text_OutputName)
	{
		Text_OutputName->SetText(Recipe->DisplayName);
	}

	if (Text_OutputDescription)
	{
		Text_OutputDescription->SetText(URetrieveItemDescriptionHelper::BuildItemDescription(
			Recipe->OutputItem.ItemId, Recipe->OutputItem.ItemCategoryTag,
			ConsumableItemTable, MaterialItemTable, WeaponDataTable));
	}

	if (Text_MaxCraftable)
	{
		Text_MaxCraftable->SetText(FText::Format(
			NSLOCTEXT("CraftPanel", "MaxCraftable", "Craftable: {0}"),
			FText::AsNumber(MaxCraftableCount)));
	}

	RefreshMaterialRows(*Recipe);

	if (Slider_CraftCount)
	{
		const float Max = FMath::Max(1.f, static_cast<float>(MaxCraftableCount));
		Slider_CraftCount->SetMaxValue(Max);
		Slider_CraftCount->SetValue(FMath::Clamp(static_cast<float>(CraftCount), 1.f, Max));
	}

	UpdateCraftCountUI();
}

void UCraftPanelWidget::RefreshMaterialRows(const FRetrieveCraftRecipeRow& Recipe)
{
	ClearMaterialRows();

	if (!VerticalBox_Materials || !MaterialRowWidgetClass)
	{
		return;
	}

	for (const FRetrieveItemStack& Material : Recipe.RequiredMaterials)
	{
		if (Material.ItemId.IsNone())
		{
			continue;
		}

		UCraftMaterialRowWidget* Row = CreateWidget<UCraftMaterialRowWidget>(
			GetOwningPlayer(), MaterialRowWidgetClass);
		if (!Row)
		{
			continue;
		}

		const int32 Owned = InventoryComponent
			? InventoryComponent->GetItemCount(Material.ItemId)
			: 0;

		Row->InitMaterialRow(ItemIconTable, MaterialItemTable, Material.ItemId,
			Material.Quantity * CraftCount, Owned);

		VerticalBox_Materials->AddChild(Row);
	}
}

void UCraftPanelWidget::ClearMaterialRows()
{
	if (VerticalBox_Materials)
	{
		VerticalBox_Materials->ClearChildren();
	}
}

void UCraftPanelWidget::UpdateCraftCountUI()
{
	if (Text_CraftCount)
	{
		Text_CraftCount->SetText(FText::AsNumber(CraftCount));
	}

	if (Button_Craft)
	{
		Button_Craft->SetIsEnabled(CanExecuteCraft());
	}
}

void UCraftPanelWidget::HandleCraftButtonClicked()
{
	ExecuteCraft();
}

void UCraftPanelWidget::HandleSliderValueChanged(float Value)
{
	CraftCount = FMath::Clamp(FMath::RoundToInt(Value), 1, FMath::Max(1, MaxCraftableCount));

	if (!SelectedRecipeId.IsNone() && CraftRecipeTable)
	{
		if (const FRetrieveCraftRecipeRow* Recipe =
			CraftRecipeTable->FindRow<FRetrieveCraftRecipeRow>(SelectedRecipeId, TEXT("CraftPanel::Slider")))
		{
			if (VerticalBox_Materials)
			{
				for (int32 i = 0; i < VerticalBox_Materials->GetChildrenCount(); ++i)
				{
					if (UCraftMaterialRowWidget* Row =
						Cast<UCraftMaterialRowWidget>(VerticalBox_Materials->GetChildAt(i)))
					{
						if (i < Recipe->RequiredMaterials.Num())
						{
							Row->InitMaterialRow(ItemIconTable, MaterialItemTable,
								Recipe->RequiredMaterials[i].ItemId,
								Recipe->RequiredMaterials[i].Quantity * CraftCount,
								InventoryComponent
									? InventoryComponent->GetItemCount(Recipe->RequiredMaterials[i].ItemId)
									: 0);
						}
					}
				}
			}
		}
	}

	UpdateCraftCountUI();
}

void UCraftPanelWidget::HandleInventoryChanged()
{
	RefreshRecipeList();

	if (SelectedRecipeId.IsNone() || !InventoryComponent)
	{
		return;
	}

	MaxCraftableCount = InventoryComponent->GetMaxCraftableCount(SelectedRecipeId);
	CraftCount = FMath::Clamp(CraftCount, 1, FMath::Max(1, MaxCraftableCount));

	if (Slider_CraftCount)
	{
		Slider_CraftCount->SetMaxValue(FMath::Max(1.f, static_cast<float>(MaxCraftableCount)));
		Slider_CraftCount->SetValue(static_cast<float>(CraftCount));
	}

	if (Text_MaxCraftable)
	{
		Text_MaxCraftable->SetText(FText::Format(
			NSLOCTEXT("CraftPanel", "MaxCraftable", "Craftable: {0}"),
			FText::AsNumber(MaxCraftableCount)));
	}

	if (VerticalBox_Materials && CraftRecipeTable)
	{
		if (const FRetrieveCraftRecipeRow* Recipe =
			CraftRecipeTable->FindRow<FRetrieveCraftRecipeRow>(SelectedRecipeId, TEXT("CraftPanel::InvChanged")))
		{
			for (int32 i = 0; i < VerticalBox_Materials->GetChildrenCount(); ++i)
			{
				if (UCraftMaterialRowWidget* Row =
					Cast<UCraftMaterialRowWidget>(VerticalBox_Materials->GetChildAt(i)))
				{
					if (i < Recipe->RequiredMaterials.Num())
					{
						const int32 Owned = InventoryComponent->GetItemCount(
							Recipe->RequiredMaterials[i].ItemId);
						Row->RefreshOwnedCount(Owned);
					}
				}
			}
		}
	}

	UpdateCraftCountUI();
}

void UCraftPanelWidget::HandleCraftCompleted(bool bSuccess, FName RecipeId, FName /*OutputItemId*/)
{
	OnCrafted.Broadcast(RecipeId, bSuccess);
	HandleInventoryChanged();
}

void UCraftPanelWidget::HandleRecipeEntryClicked(FName RecipeId)
{
	SelectRecipe(RecipeId);
}
