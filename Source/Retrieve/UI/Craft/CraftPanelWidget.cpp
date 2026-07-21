#include "UI/Craft/CraftPanelWidget.h"

#include "UI/Bonfire/BonfireMenuWidget.h"
#include "UI/Craft/CraftMaterialRowWidget.h"
#include "UI/Craft/CraftRecipeEntryWidget.h"
#include "UI/RetrieveItemDescriptionHelper.h"
#include "Components/Inventory/InventoryComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameplayTags/RetrieveGameplayTags.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"

const FLinearColor UCraftPanelWidget::CraftButtonEnabledColor(1.0f, 0.87f, 0.45f, 1.0f);
const FLinearColor UCraftPanelWidget::CraftButtonDisabledColor(0.5f, 0.5f, 0.5f, 0.6f);

namespace
{
	// 설명 첫 줄(아이템 이름)은 별도 이름 라벨과 중복되므로 제거한다.
	FText StripLeadingNameLine(const FText& InText)
	{
		const FString Str = InText.ToString();
		int32 NewlineIdx = INDEX_NONE;
		if (Str.FindChar(TEXT('\n'), NewlineIdx))
		{
			return FText::FromString(Str.Mid(NewlineIdx + 1));
		}
		return InText;
	}

	// 첫 줄만 반환 (목록 행은 컴팩트하게 설명 한 줄만 표시)
	FText FirstLineOnly(const FText& InText)
	{
		const FString Str = InText.ToString();
		int32 NewlineIdx = INDEX_NONE;
		if (Str.FindChar(TEXT('\n'), NewlineIdx))
		{
			return FText::FromString(Str.Left(NewlineIdx));
		}
		return InText;
	}
}

void UCraftPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ResolveDefaultDataAssets();
	ApplyStaticTexts();
	BindButtonEvents();

	if (ScrollBox_RecipeList)
	{
		ScrollBox_RecipeList->SetClipping(EWidgetClipping::ClipToBoundsAlways);
	}

	// 상세 패널은 고정 우측 컬럼으로 상시 표시
	if (VerticalBox_CraftRoot)
	{
		VerticalBox_CraftRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	RefreshCategoryButtons();
	RefreshRecipeList();
	SelectFirstVisibleRecipe();
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

	// UI와 인벤토리가 서로 다른 제작 테이블을 참조하면 화면에는 재료가 충분해 보여도
	// GetMaxCraftableCount/CraftItem은 다른 행을 검사하게 된다. 패널에서 확정한 테이블을
	// 실제 제작 판정에도 동일하게 사용해 표시와 실행 결과를 맞춘다.
	if (CraftRecipeTable && InventoryComponent->CraftRecipeTable != CraftRecipeTable)
	{
		InventoryComponent->CraftRecipeTable = CraftRecipeTable;
	}

	RefreshRecipeList();
	SelectFirstVisibleRecipe();
}

void UCraftPanelWidget::SetActiveCategory(ECraftCategory InCategory)
{
	if (ActiveCategory == InCategory)
	{
		return;
	}

	ActiveCategory = InCategory;
	RefreshCategoryButtons();
	RefreshRecipeList();
	SelectFirstVisibleRecipe();
}

void UCraftPanelWidget::RefreshCategoryButtons()
{
	auto Apply = [&](UImage* Icon, ECraftCategory Category)
	{
		if (Icon)
		{
			Icon->SetColorAndOpacity(
				Category == ActiveCategory ? CategoryIconSelectedColor : CategoryIconNormalColor);
		}
	};

	Apply(Img_Btn_Consumable, ECraftCategory::Consumable);
	Apply(Img_Btn_Buff, ECraftCategory::Buff);
	Apply(Img_Btn_Material, ECraftCategory::Material);
	Apply(Img_Btn_Equip, ECraftCategory::Equipment);
	Apply(Img_Btn_Etc, ECraftCategory::Etc);
}

void UCraftPanelWidget::SelectRecipe(FName InRecipeId)
{
	if (!CraftRecipeTable || InRecipeId.IsNone())
	{
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
	bSelectedRecipeIsEnhancement = !Recipe->UpgradeTargetItem.ItemId.IsNone();
	CraftCount = bSelectedRecipeIsEnhancement
		? FMath::Min(1, FMath::Max(0, MaxCraftableCount))
		: FMath::Clamp(CraftCount, 1, FMath::Max(1, MaxCraftableCount));

	RefreshDetailPanel();
	RefreshRecipeEntrySelection();
}

bool UCraftPanelWidget::CanExecuteCraft() const
{
	return !bIsCrafting
		&& InventoryComponent
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

	BeginTimedCraft();
	return true;
}

void UCraftPanelWidget::BeginTimedCraft()
{
	const FRetrieveCraftRecipeRow* Recipe = CraftRecipeTable
		? CraftRecipeTable->FindRow<FRetrieveCraftRecipeRow>(SelectedRecipeId, TEXT("CraftPanel::BeginTimedCraft"))
		: nullptr;
	if (!Recipe)
	{
		return;
	}

	bIsCrafting = true;
	PendingRecipeId = SelectedRecipeId;
	PendingCraftCount = CraftCount;
	PendingCraftCategory = ActiveCategory;
	UpdateCraftCountUI();

	// TimedActionWidget은 별도로 띄우지 않고 WBP_BonfireMenu에 내장된 인스턴스를 사용한다
	// (Panel_ConfirmOverwrite와 동일한 패턴 — AddToViewport로는 항상 부모 캔버스 전체 크기로 늘어났다).
	if (UBonfireMenuWidget* BonfireMenu = GetTypedOuter<UBonfireMenuWidget>())
	{
		FSimpleDelegate OnComplete;
		OnComplete.BindUObject(this, &ThisClass::HandleTimedCraftComplete);
		BonfireMenu->ShowTimedAction(Recipe->CraftDuration, Recipe->DisplayName, OnComplete);
	}
	else
	{
		// BonfireMenu를 못 찾으면 즉시 완료 처리(폴백)
		HandleTimedCraftComplete();
	}
}

void UCraftPanelWidget::HandleTimedCraftComplete()
{
	if (UBonfireMenuWidget* BonfireMenu = GetTypedOuter<UBonfireMenuWidget>())
	{
		BonfireMenu->HideTimedAction();
	}

	const int32 CountToCraft = PendingCraftCount;

	// 확률제 레시피를 2개 이상 제작하면 매 회차 결과 팝업이 한 프레임에 서로 덮어써져 마지막 것만 보인다.
	// 배치 동안에는 HandleCraftCompleted에서 개별 팝업을 억제하고 성공/실패만 누적한 뒤,
	// 루프 종료 후 요약 팝업("성공 N / 실패 M")을 1회만 띄운다.
	const FRetrieveCraftRecipeRow* PendingRecipe = CraftRecipeTable
		? CraftRecipeTable->FindRow<FRetrieveCraftRecipeRow>(PendingRecipeId, TEXT("CraftPanel::TimedComplete"))
		: nullptr;
	const bool bProbabilistic = PendingRecipe && PendingRecipe->SuccessChance < 1.0f;
	bBatchResultInProgress = bProbabilistic && CountToCraft > 1;
	BatchSuccessCount = 0;
	BatchFailCount = 0;

	bool bCraftedAny = false;
	if (InventoryComponent)
	{
		for (int32 i = 0; i < CountToCraft; ++i)
		{
			// 재료가 남아 있는 한 계속 시도한다. 확률 실패(재료는 소모, 결과만 실패)로는 중단하지 않고,
			// 재료가 실제로 소진됐을 때에만 멈춘다.
			if (!InventoryComponent->CanCraftItem(PendingRecipeId))
			{
				break;
			}
			if (InventoryComponent->CraftItem(PendingRecipeId))
			{
				bCraftedAny = true;
			}
		}
	}

	if (bCraftedAny)
	{
		if (PendingCraftCategory == ECraftCategory::Consumable || PendingCraftCategory == ECraftCategory::Buff)
		{
			PlayContextUISound(RetrieveGameplayTags::UI_Sound_Craft_Consumable, ERetrieveUISoundEvent::Release);
		}
		else if (PendingCraftCategory == ECraftCategory::Equipment)
		{
			PlayContextUISound(RetrieveGameplayTags::UI_Sound_Craft_Equipment, ERetrieveUISoundEvent::Release);
		}
	}

	// 배치 요약 팝업 (개별 팝업은 HandleCraftCompleted에서 억제됨)
	if (bBatchResultInProgress)
	{
		bBatchResultInProgress = false;
		if (UBonfireMenuWidget* BonfireMenu = GetTypedOuter<UBonfireMenuWidget>())
		{
			const bool bAnySuccess = BatchSuccessCount > 0;
			BonfireMenu->ShowCraftResultSummary(BatchSuccessCount, BatchFailCount, LoadCraftResultIcon(bAnySuccess));
		}
	}

	bIsCrafting = false;
	UpdateCraftCountUI();
}

UTexture2D* UCraftPanelWidget::LoadCraftResultIcon(bool bSuccess) const
{
	const TCHAR* IconPath = bSuccess
		? TEXT("/Game/External/UIFantasyWarriorHUD/Textures/Icons_Inventory/T_ICON_FantasyWarrior_Inventory_Swords01_Underlay.T_ICON_FantasyWarrior_Inventory_Swords01_Underlay")
		: TEXT("/Game/External/UIFantasyWarriorHUD/Textures/Icons_Status/T_ICON_FantasyWarrior_Status_AttackBroken01_Stroke.T_ICON_FantasyWarrior_Status_AttackBroken01_Stroke");
	return LoadObject<UTexture2D>(nullptr, IconPath);
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

	if (Button_CountMinus && !Button_CountMinus->OnClicked.IsBound())
	{
		Button_CountMinus->OnClicked.AddDynamic(this, &UCraftPanelWidget::HandleCountMinusClicked);
	}
	if (Button_CountPlus && !Button_CountPlus->OnClicked.IsBound())
	{
		Button_CountPlus->OnClicked.AddDynamic(this, &UCraftPanelWidget::HandleCountPlusClicked);
	}
	if (Button_CountMax && !Button_CountMax->OnClicked.IsBound())
	{
		Button_CountMax->OnClicked.AddDynamic(this, &UCraftPanelWidget::HandleCountMaxClicked);
	}

	if (Button_Cat_Consumable && !Button_Cat_Consumable->OnClicked.IsBound())
	{
		Button_Cat_Consumable->OnClicked.AddDynamic(this, &UCraftPanelWidget::HandleCatConsumableClicked);
	}
	if (Button_Cat_Buff && !Button_Cat_Buff->OnClicked.IsBound())
	{
		Button_Cat_Buff->OnClicked.AddDynamic(this, &UCraftPanelWidget::HandleCatBuffClicked);
	}
	if (Button_Cat_Material && !Button_Cat_Material->OnClicked.IsBound())
	{
		Button_Cat_Material->OnClicked.AddDynamic(this, &UCraftPanelWidget::HandleCatMaterialClicked);
	}
	if (Button_Cat_Equip && !Button_Cat_Equip->OnClicked.IsBound())
	{
		Button_Cat_Equip->OnClicked.AddDynamic(this, &UCraftPanelWidget::HandleCatEquipClicked);
	}
	if (Button_Cat_Etc && !Button_Cat_Etc->OnClicked.IsBound())
	{
		Button_Cat_Etc->OnClicked.AddDynamic(this, &UCraftPanelWidget::HandleCatEtcClicked);
	}

	RegisterSoundButton(Button_Craft);
	RegisterSoundButton(Button_CountMinus);
	RegisterSoundButton(Button_CountPlus);
	RegisterSoundButton(Button_CountMax);
	RegisterSoundButton(Button_Cat_Consumable);
	RegisterSoundButton(Button_Cat_Buff);
	RegisterSoundButton(Button_Cat_Material);
	RegisterSoundButton(Button_Cat_Equip);
	RegisterSoundButton(Button_Cat_Etc);
}

void UCraftPanelWidget::ApplyStaticTexts()
{
	if (Text_CraftCountLabel)
	{
		Text_CraftCountLabel->SetText(NSLOCTEXT("CraftPanel", "CraftCountLabel", "제작 수량"));
	}

	if (Text_CraftButton)
	{
		Text_CraftButton->SetText(NSLOCTEXT("CraftPanel", "CraftButton", "제작하기"));
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

	// 카테고리 필터 태그 기본값 (디자이너가 비워두면 자동 채움)
	if (!Tag_Consumable.IsValid())
	{
		Tag_Consumable = FGameplayTag::RequestGameplayTag(FName("Item.Consumable"), false);
	}
	if (!Tag_Buff.IsValid())
	{
		Tag_Buff = FGameplayTag::RequestGameplayTag(FName("Item.Consumable.Buff"), false);
	}
	if (!Tag_Material.IsValid())
	{
		Tag_Material = FGameplayTag::RequestGameplayTag(FName("Item.Material"), false);
	}
	if (Tags_Equipment.IsEmpty())
	{
		const FGameplayTag WeaponTag = FGameplayTag::RequestGameplayTag(FName("Item.Weapon"), false);
		const FGameplayTag ArmorTag = FGameplayTag::RequestGameplayTag(FName("Item.Armor"), false);
		if (WeaponTag.IsValid())
		{
			Tags_Equipment.AddTag(WeaponTag);
		}
		if (ArmorTag.IsValid())
		{
			Tags_Equipment.AddTag(ArmorTag);
		}
	}
}

bool UCraftPanelWidget::PassesCategoryFilter(const FRetrieveCraftRecipeRow& Recipe) const
{
	const FGameplayTag& Cat = Recipe.OutputItem.ItemCategoryTag;

	switch (ActiveCategory)
	{
	case ECraftCategory::Consumable:
		return Tag_Consumable.IsValid() && Cat.MatchesTag(Tag_Consumable);
	case ECraftCategory::Buff:
		return Tag_Buff.IsValid() && Cat.MatchesTag(Tag_Buff);
	case ECraftCategory::Material:
		return Tag_Material.IsValid() && Cat.MatchesTag(Tag_Material);
	case ECraftCategory::Equipment:
		return !Tags_Equipment.IsEmpty() && Cat.MatchesAny(Tags_Equipment);
	case ECraftCategory::Etc:
	default:
		return !(Tag_Consumable.IsValid() && Cat.MatchesTag(Tag_Consumable))
			&& !(Tag_Material.IsValid() && Cat.MatchesTag(Tag_Material))
			&& !(!Tags_Equipment.IsEmpty() && Cat.MatchesAny(Tags_Equipment));
	}
}

void UCraftPanelWidget::SelectFirstVisibleRecipe()
{
	if (RecipeListOrder.Num() == 0)
	{
		// 보이는 레시피가 없으면 상세를 비운다
		SelectedRecipeId = NAME_None;
		MaxCraftableCount = 0;
		bSelectedRecipeIsEnhancement = false;
		ClearMaterialRows();
		if (Text_OutputName)
		{
			Text_OutputName->SetText(FText::GetEmpty());
		}
		if (Text_OutputDescription)
		{
			Text_OutputDescription->SetText(FText::GetEmpty());
		}
		if (Text_MaxCraftable)
		{
			Text_MaxCraftable->SetText(FText::GetEmpty());
		}
		UpdateCraftCountUI();
		return;
	}

	// 현재 선택이 보이는 목록에 있으면 유지, 아니면 첫 항목 선택
	if (SelectedRecipeId.IsNone() || !RecipeListOrder.Contains(SelectedRecipeId))
	{
		CraftCount = 1;
		SelectRecipe(RecipeListOrder[0]);
	}
	else
	{
		SelectRecipe(SelectedRecipeId);
	}
}

void UCraftPanelWidget::RefreshRecipeList()
{
	if (!ScrollBox_RecipeList || !CraftRecipeTable)
	{
		return;
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
			if (PassesCategoryFilter(*Recipe))
			{
				AddRecipeEntry(RecipeId, *Recipe);
			}
		}
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

	const int32 OwnedOutput = InventoryComponent
		? InventoryComponent->GetItemCount(Recipe.OutputItem.ItemId)
		: 0;
	RecipeEntry->InitRecipeEntry(
		RecipeId,
		Recipe.DisplayName.IsEmpty() ? FText::FromName(RecipeId) : Recipe.DisplayName,
		OwnedOutput);
	// 목록 행은 이름 라벨 + 설명 한 줄(스탯 제외)만 컴팩트하게 표시
	RecipeEntry->SetDescription(FirstLineOnly(StripLeadingNameLine(URetrieveItemDescriptionHelper::BuildItemDescription(
		Recipe.OutputItem.ItemId, Recipe.OutputItem.ItemCategoryTag,
		ConsumableItemTable, MaterialItemTable, WeaponDataTable))));

	if (ItemIconTable)
	{
		if (const FRetrieveItemIconRow* IconRow =
			ItemIconTable->FindRow<FRetrieveItemIconRow>(Recipe.OutputItem.ItemId, TEXT("CraftPanel::EntryIcon"), false))
		{
			if (UTexture2D* Tex = IconRow->IconTexture.LoadSynchronous())
			{
				RecipeEntry->SetIconTexture(Tex);
			}
		}
	}

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
		EntrySlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		EntrySlot->SetHorizontalAlignment(HAlign_Fill);
		EntrySlot->SetVerticalAlignment(VAlign_Top);
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
		Text_OutputDescription->SetText(StripLeadingNameLine(URetrieveItemDescriptionHelper::BuildItemDescription(
			Recipe->OutputItem.ItemId, Recipe->OutputItem.ItemCategoryTag,
			ConsumableItemTable, MaterialItemTable, WeaponDataTable)));
	}

	if (Text_MaxCraftable)
	{
		const int32 OwnedOutput = InventoryComponent
			? InventoryComponent->GetItemCount(Recipe->OutputItem.ItemId)
			: 0;
		Text_MaxCraftable->SetText(FText::Format(
			NSLOCTEXT("CraftPanel", "OwnedOutput", "보유: {0}"),
			FText::AsNumber(OwnedOutput)));
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

	// 강화 레시피는 "무엇을 강화하는지" 보여주기 위해 원본 장비를 재료 목록 맨 위에 표시한다.
	// (RequiredMaterials와 달리 소모 여부는 성공/실패에 따라 InventoryComponent::CraftItem이 처리한다)
	if (!Recipe.UpgradeTargetItem.ItemId.IsNone())
	{
		if (UCraftMaterialRowWidget* UpgradeRow = CreateWidget<UCraftMaterialRowWidget>(
			GetOwningPlayer(), MaterialRowWidgetClass))
		{
			const int32 Owned = InventoryComponent
				? InventoryComponent->GetItemCount(Recipe.UpgradeTargetItem.ItemId)
				: 0;
			UpgradeRow->InitMaterialRow(ItemIconTable, MaterialItemTable, Recipe.UpgradeTargetItem.ItemId,
				Recipe.UpgradeTargetItem.Quantity, Owned, WeaponDataTable);
			VerticalBox_Materials->AddChild(UpgradeRow);
		}
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

		// 필요량은 실제 제작 횟수 기준으로 표시한다. 강화 레시피는 제작 불가 시 CraftCount가 0으로
		// 떨어지는데, 그대로 곱하면 필요량이 0이 되어 재료 행이 "부족한데도 충분한 것처럼" 보인다.
		// Max(1, CraftCount)로 최소 1회분(원본 Quantity)을 항상 반영해 표시를 실제와 맞춘다.
		Row->InitMaterialRow(ItemIconTable, MaterialItemTable, Material.ItemId,
			Material.Quantity * FMath::Max(1, CraftCount), Owned, WeaponDataTable);

		VerticalBox_Materials->AddChild(Row);
	}

	// 강화 확률은 재료 목록과 제작 수량 사이에 표시한다(VerticalBox_Materials의 마지막 줄).
	if (Recipe.SuccessChance < 1.0f)
	{
		if (UTextBlock* SuccessChanceText = WidgetTree
			? WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass())
			: nullptr)
		{
			const int32 SuccessPercent = FMath::RoundToInt(Recipe.SuccessChance * 100.0f);
			SuccessChanceText->SetText(FText::Format(
				NSLOCTEXT("CraftPanel", "SuccessChance", "성공 확률: {0}%"),
				FText::AsNumber(SuccessPercent)));
			SuccessChanceText->SetColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.78f, 0.95f, 1.0f)));
			SuccessChanceText->SetJustification(ETextJustify::Center);
			VerticalBox_Materials->AddChild(SuccessChanceText);
		}
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

	const bool bCanCraft = CanExecuteCraft();
	if (Button_Craft)
	{
		Button_Craft->SetIsEnabled(bCanCraft);
	}
	ApplyCraftButtonEnabledStyle(bCanCraft);

	// 장비 강화 레시피는 1회 1개 단위로만 진행하므로 수량 스테퍼를 잠근다.
	const bool bHasSelection = !SelectedRecipeId.IsNone() && !bSelectedRecipeIsEnhancement;
	if (Button_CountMinus)
	{
		Button_CountMinus->SetIsEnabled(bHasSelection && CraftCount > 1);
	}
	if (Button_CountPlus)
	{
		Button_CountPlus->SetIsEnabled(bHasSelection && CraftCount < MaxCraftableCount);
	}
	if (Button_CountMax)
	{
		Button_CountMax->SetIsEnabled(bHasSelection && CraftCount < MaxCraftableCount);
	}
}

void UCraftPanelWidget::ApplyCraftButtonEnabledStyle(bool bEnabled)
{
	if (Text_CraftButton)
	{
		Text_CraftButton->SetColorAndOpacity(FSlateColor(
			bEnabled ? CraftButtonEnabledColor : CraftButtonDisabledColor));
	}
}

void UCraftPanelWidget::SetCraftCount(int32 NewCount)
{
	const int32 Clamped = FMath::Clamp(NewCount, 1, FMath::Max(1, MaxCraftableCount));
	CraftCount = Clamped;

	// 재료 행 필요 수량 갱신
	if (!SelectedRecipeId.IsNone() && CraftRecipeTable && VerticalBox_Materials)
	{
		if (const FRetrieveCraftRecipeRow* Recipe =
			CraftRecipeTable->FindRow<FRetrieveCraftRecipeRow>(SelectedRecipeId, TEXT("CraftPanel::SetCount")))
		{
			// 강화 대상 행/성공 확률 표시까지 함께 있어 인덱스 기반 패치는 어긋나기 쉬우므로 전체를 다시 그린다.
			RefreshMaterialRows(*Recipe);
		}
	}

	if (Slider_CraftCount)
	{
		Slider_CraftCount->SetValue(static_cast<float>(CraftCount));
	}

	UpdateCraftCountUI();
}

void UCraftPanelWidget::HandleCraftButtonClicked()
{
	ExecuteCraft();
}

void UCraftPanelWidget::HandleSliderValueChanged(float Value)
{
	SetCraftCount(FMath::RoundToInt(Value));
}

void UCraftPanelWidget::HandleCountMinusClicked()
{
	SetCraftCount(CraftCount - 1);
}

void UCraftPanelWidget::HandleCountPlusClicked()
{
	SetCraftCount(CraftCount + 1);
}

void UCraftPanelWidget::HandleCountMaxClicked()
{
	SetCraftCount(FMath::Max(1, MaxCraftableCount));
}

void UCraftPanelWidget::HandleCatConsumableClicked()
{
	SetActiveCategory(ECraftCategory::Consumable);
}

void UCraftPanelWidget::HandleCatBuffClicked()
{
	SetActiveCategory(ECraftCategory::Buff);
}

void UCraftPanelWidget::HandleCatMaterialClicked()
{
	SetActiveCategory(ECraftCategory::Material);
}

void UCraftPanelWidget::HandleCatEquipClicked()
{
	SetActiveCategory(ECraftCategory::Equipment);
}

void UCraftPanelWidget::HandleCatEtcClicked()
{
	SetActiveCategory(ECraftCategory::Etc);
}

void UCraftPanelWidget::HandleInventoryChanged()
{
	RefreshRecipeList();

	if (SelectedRecipeId.IsNone() || !InventoryComponent)
	{
		SelectFirstVisibleRecipe();
		return;
	}

	if (!RecipeListOrder.Contains(SelectedRecipeId))
	{
		SelectFirstVisibleRecipe();
		return;
	}

	MaxCraftableCount = InventoryComponent->GetMaxCraftableCount(SelectedRecipeId);
	CraftCount = FMath::Clamp(CraftCount, 1, FMath::Max(1, MaxCraftableCount));

	RefreshDetailPanel();
	RefreshRecipeEntrySelection();
}

void UCraftPanelWidget::HandleCraftCompleted(bool bSuccess, FName RecipeId, FName /*OutputItemId*/)
{
	OnCrafted.Broadcast(RecipeId, bSuccess);
	HandleInventoryChanged();

	// 강화 레시피(확률제)만 성공/실패 팝업을 띄운다. 일반(확정) 제작은 조용히 갱신한다.
	const FRetrieveCraftRecipeRow* Recipe = CraftRecipeTable
		? CraftRecipeTable->FindRow<FRetrieveCraftRecipeRow>(RecipeId, TEXT("CraftPanel::HandleCraftCompleted"))
		: nullptr;
	if (!Recipe || Recipe->SuccessChance >= 1.0f)
	{
		return;
	}

	// 배치 제작 중에는 개별 팝업을 억제하고 성공/실패만 누적한다.
	// (요약 팝업은 HandleTimedCraftComplete가 루프 종료 후 1회 띄운다)
	if (bBatchResultInProgress)
	{
		if (bSuccess) { ++BatchSuccessCount; } else { ++BatchFailCount; }
		return;
	}

	// 단일 강화: 즉시 성공/실패 팝업
	if (UBonfireMenuWidget* BonfireMenu = GetTypedOuter<UBonfireMenuWidget>())
	{
		BonfireMenu->ShowCraftResult(bSuccess, LoadCraftResultIcon(bSuccess));
	}
}

void UCraftPanelWidget::HandleRecipeEntryClicked(FName RecipeId)
{
	SelectRecipe(RecipeId);
}
