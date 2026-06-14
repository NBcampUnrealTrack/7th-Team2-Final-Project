#include "UI/VFX/RetrieveUIVFXEditorUtility.h"

#include "Curves/CurveFloat.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "UI/VFX/RetrieveUIVFXProfile.h"

#if WITH_EDITOR
#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/ScaleBox.h"
#include "Components/Slider.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Factories/DataTableFactory.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UI/Bonfire/BonfireMenuWidget.h"
#include "UI/Craft/CraftMaterialRowWidget.h"
#include "UI/Craft/CraftPanelWidget.h"
#include "UI/Craft/CraftRecipeEntryWidget.h"
#include "UI/HUD/RetrieveNormalMonsterHealthBarWidget.h"
#include "Data/Interaction/RetrieveInteractionPresetProfileAsset.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"
#endif

namespace
{
UCurveFloat* LoadCurve(const TCHAR* Path)
{
	return LoadObject<UCurveFloat>(nullptr, Path);
}

#if WITH_EDITOR
FAssetToolsModule& AssetTools()
{
	return FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
}

UObject* LoadAsset(const TCHAR* Path)
{
	return StaticLoadObject(UObject::StaticClass(), nullptr, Path);
}

UWidgetBlueprint* LoadWidgetBlueprint(const TCHAR* Path)
{
	return Cast<UWidgetBlueprint>(LoadAsset(Path));
}

UWidgetBlueprint* CreateWidgetBlueprintIfMissing(const FString& PackagePath, const FString& AssetName, TSubclassOf<UUserWidget> ParentClass)
{
	const FString ObjectPath = PackagePath / AssetName + TEXT(".") + AssetName;
	if (UWidgetBlueprint* Existing = LoadWidgetBlueprint(*ObjectPath))
	{
		return Existing;
	}

	UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
	Factory->ParentClass = ParentClass;
	UObject* Created = AssetTools().Get().CreateAsset(AssetName, PackagePath, UWidgetBlueprint::StaticClass(), Factory);
	return Cast<UWidgetBlueprint>(Created);
}

UDataTable* CreateDataTableIfMissing(const FString& PackagePath, const FString& AssetName, UScriptStruct* RowStruct)
{
	const FString ObjectPath = PackagePath / AssetName + TEXT(".") + AssetName;
	if (UDataTable* Existing = Cast<UDataTable>(LoadAsset(*ObjectPath)))
	{
		return Existing;
	}

	UDataTableFactory* Factory = NewObject<UDataTableFactory>();
	Factory->Struct = RowStruct;
	UObject* Created = AssetTools().Get().CreateAsset(AssetName, PackagePath, UDataTable::StaticClass(), Factory);
	return Cast<UDataTable>(Created);
}

template <typename T>
T* ConstructNamed(UWidgetTree* Tree, FName Name)
{
	T* Widget = Tree ? Tree->ConstructWidget<T>(T::StaticClass(), Name) : nullptr;
	if (Widget && !Name.IsNone())
	{
		Widget->bIsVariable = true;
	}
	return Widget;
}

void SetFontSize(UTextBlock* TextBlock, int32 Size)
{
	if (!TextBlock)
	{
		return;
	}

	FSlateFontInfo Font = TextBlock->GetFont();
	Font.Size = Size;
	TextBlock->SetFont(Font);
}

void SetCanvasSlot(UWidget* Widget, const FAnchors& Anchors, const FVector2D& Position, const FVector2D& Size, const FVector2D& Alignment, int32 ZOrder = 0)
{
	if (UCanvasPanelSlot* Slot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr)
	{
		Slot->SetAnchors(Anchors);
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		Slot->SetAlignment(Alignment);
		Slot->SetZOrder(ZOrder);
	}
}

void EnsureWidgetVariableGuids(UWidgetBlueprint* BP)
{
	if (!BP || !BP->WidgetTree)
	{
		return;
	}

	TArray<UWidget*> Widgets;
	BP->WidgetTree->GetAllWidgets(Widgets);
	TSet<FName> SeenNames;
	for (UWidget* Widget : Widgets)
	{
		if (!Widget)
		{
			continue;
		}

		SeenNames.Add(Widget->GetFName());
		Widget->bIsVariable = true;
		if (!BP->WidgetVariableNameToGuidMap.Contains(Widget->GetFName()))
		{
			BP->WidgetVariableNameToGuidMap.Add(Widget->GetFName(), FGuid::NewGuid());
		}
	}

	for (auto It = BP->WidgetVariableNameToGuidMap.CreateIterator(); It; ++It)
	{
		if (!SeenNames.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
	}
}

void ResetWidgetTree(UWidgetBlueprint* BP)
{
	if (!BP || !BP->WidgetTree)
	{
		return;
	}

	TArray<UWidget*> Widgets;
	BP->WidgetTree->GetAllWidgets(Widgets);
	for (UWidget* Widget : Widgets)
	{
		if (Widget)
		{
			BP->WidgetTree->RemoveWidget(Widget);
		}
	}

	BP->WidgetTree->RootWidget = nullptr;
	BP->WidgetVariableNameToGuidMap.Empty();
}

void SetSizeBox(UWidget* Widget, float Width, float Height)
{
	if (USizeBox* SizeBox = Cast<USizeBox>(Widget))
	{
		SizeBox->SetWidthOverride(Width);
		SizeBox->SetHeightOverride(Height);
	}
}

UTextBlock* MakeText(UWidgetTree* Tree, FName Name, const FText& Text, int32 FontSize, FLinearColor Color = FLinearColor::White)
{
	UTextBlock* TextBlock = ConstructNamed<UTextBlock>(Tree, Name);
	if (TextBlock)
	{
		TextBlock->SetText(Text);
		SetFontSize(TextBlock, FontSize);
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
	}
	return TextBlock;
}

UButton* MakeButtonWithText(UWidgetTree* Tree, FName ButtonName, FName TextName, const FText& Text)
{
	UButton* Button = ConstructNamed<UButton>(Tree, ButtonName);
	UTextBlock* TextBlock = MakeText(Tree, TextName, Text, 14, FLinearColor(1.0f, 0.88f, 0.63f, 1.0f));
	if (Button && TextBlock)
	{
		Button->AddChild(TextBlock);
	}
	return Button;
}

void CompileAndDirty(UWidgetBlueprint* BP)
{
	if (!BP)
	{
		return;
	}

	BP->Modify();
	EnsureWidgetVariableGuids(BP);
	BP->MarkPackageDirty();
	FKismetEditorUtilities::CompileBlueprint(BP);
}

void ConfigureCraftMaterialRow()
{
	UWidgetBlueprint* BP = CreateWidgetBlueprintIfMissing(
		TEXT("/Game/Retrieve/UI/Craft"),
		TEXT("WBP_CraftMaterialRow"),
		UCraftMaterialRowWidget::StaticClass());
	if (!BP || !BP->WidgetTree)
	{
		return;
	}

	BP->WidgetTree->Modify();
	ResetWidgetTree(BP);
	UHorizontalBox* Root = ConstructNamed<UHorizontalBox>(BP->WidgetTree, TEXT("Root"));
	BP->WidgetTree->RootWidget = Root;

	USizeBox* IconSize = ConstructNamed<USizeBox>(BP->WidgetTree, TEXT("SizeBox_MatIcon"));
	UImage* Icon = ConstructNamed<UImage>(BP->WidgetTree, TEXT("Image_MatIcon"));
	UTextBlock* Name = MakeText(BP->WidgetTree, TEXT("Text_MatName"), FText::FromString(TEXT("재료")), 13);
	UTextBlock* Count = MakeText(BP->WidgetTree, TEXT("Text_MatCount"), FText::FromString(TEXT("x0")), 13);

	if (IconSize && Icon)
	{
		IconSize->SetWidthOverride(32.0f);
		IconSize->SetHeightOverride(32.0f);
		IconSize->AddChild(Icon);
		Root->AddChildToHorizontalBox(IconSize);
	}

	if (Name)
	{
		Name->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (UHorizontalBoxSlot* Slot = Root->AddChildToHorizontalBox(Name))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetPadding(FMargin(8.0f, 0.0f));
		}
	}

	if (Count)
	{
		Count->SetJustification(ETextJustify::Left);
		if (UHorizontalBoxSlot* Slot = Root->AddChildToHorizontalBox(Count))
		{
			Slot->SetPadding(FMargin(6.0f, 0.0f, 10.0f, 0.0f));
		}
	}

	CompileAndDirty(BP);
}

void ConfigureCraftRecipeEntry()
{
	UWidgetBlueprint* BP = CreateWidgetBlueprintIfMissing(
		TEXT("/Game/Retrieve/UI/Craft"),
		TEXT("WBP_CraftRecipeEntry"),
		UCraftRecipeEntryWidget::StaticClass());
	if (!BP || !BP->WidgetTree)
	{
		return;
	}

	BP->WidgetTree->Modify();
	ResetWidgetTree(BP);
	UButton* Root = ConstructNamed<UButton>(BP->WidgetTree, TEXT("Button_Recipe"));
	BP->WidgetTree->RootWidget = Root;

	UHorizontalBox* Content = ConstructNamed<UHorizontalBox>(BP->WidgetTree, TEXT("HBox_RecipeContent"));
	Root->AddChild(Content);

	UTextBlock* Name = MakeText(BP->WidgetTree, TEXT("Text_RecipeName"), FText::FromString(TEXT("Recipe")), 14, FLinearColor(0.95f, 0.88f, 0.72f, 1.0f));
	UTextBlock* Count = MakeText(BP->WidgetTree, TEXT("Text_CraftableCount"), FText::FromString(TEXT("x0")), 13, FLinearColor(0.76f, 0.92f, 0.64f, 1.0f));

	if (Name)
	{
		Name->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (UHorizontalBoxSlot* Slot = Content->AddChildToHorizontalBox(Name))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetPadding(FMargin(10.0f, 6.0f, 8.0f, 6.0f));
		}
	}

	if (Count)
	{
		Count->SetJustification(ETextJustify::Right);
		Count->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		if (UHorizontalBoxSlot* Slot = Content->AddChildToHorizontalBox(Count))
		{
			Slot->SetPadding(FMargin(8.0f, 6.0f, 10.0f, 6.0f));
		}
	}

	CompileAndDirty(BP);
}

void ConfigureCraftPanel()
{
	UWidgetBlueprint* BP = CreateWidgetBlueprintIfMissing(
		TEXT("/Game/Retrieve/UI/Craft"),
		TEXT("WBP_CraftPanel"),
		UCraftPanelWidget::StaticClass());
	if (!BP || !BP->WidgetTree)
	{
		return;
	}

	BP->WidgetTree->Modify();
	ResetWidgetTree(BP);
	UVerticalBox* Root = ConstructNamed<UVerticalBox>(BP->WidgetTree, TEXT("Root_CraftPanel"));
	BP->WidgetTree->RootWidget = Root;

	UScrollBox* RecipeList = ConstructNamed<UScrollBox>(BP->WidgetTree, TEXT("ScrollBox_RecipeList"));
	if (UVerticalBoxSlot* RecipeListSlot = Root->AddChildToVerticalBox(RecipeList))
	{
		RecipeListSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UVerticalBox* Detail = ConstructNamed<UVerticalBox>(BP->WidgetTree, TEXT("VerticalBox_CraftRoot"));
	Detail->SetVisibility(ESlateVisibility::Collapsed);
	Root->AddChildToVerticalBox(Detail);

	UHorizontalBox* OutputRow = ConstructNamed<UHorizontalBox>(BP->WidgetTree, TEXT("HBox_Output"));
	Detail->AddChildToVerticalBox(OutputRow);

	USizeBox* OutputIconBox = ConstructNamed<USizeBox>(BP->WidgetTree, TEXT("SizeBox_OutputIcon"));
	UImage* OutputIcon = ConstructNamed<UImage>(BP->WidgetTree, TEXT("Image_OutputIcon"));
	OutputIconBox->SetWidthOverride(64.0f);
	OutputIconBox->SetHeightOverride(64.0f);
	OutputIconBox->AddChild(OutputIcon);
	OutputRow->AddChildToHorizontalBox(OutputIconBox);

	UVerticalBox* OutputTexts = ConstructNamed<UVerticalBox>(BP->WidgetTree, TEXT("VBox_OutputTexts"));
	OutputRow->AddChildToHorizontalBox(OutputTexts);
	OutputTexts->AddChildToVerticalBox(MakeText(BP->WidgetTree, TEXT("Text_OutputName"), FText::FromString(TEXT("제작품")), 16));
	OutputTexts->AddChildToVerticalBox(MakeText(BP->WidgetTree, TEXT("Text_MaxCraftable"), FText::FromString(TEXT("최대 0개 제작 가능")), 13, FLinearColor(0.72f, 0.72f, 0.72f, 1.0f)));
	OutputTexts->AddChildToVerticalBox(MakeText(BP->WidgetTree, TEXT("Text_OutputDescription"), FText::GetEmpty(), 12, FLinearColor(0.85f, 0.82f, 0.74f, 1.0f)));

	UVerticalBox* Materials = ConstructNamed<UVerticalBox>(BP->WidgetTree, TEXT("VerticalBox_Materials"));
	Detail->AddChildToVerticalBox(Materials);

	UHorizontalBox* CountRow = ConstructNamed<UHorizontalBox>(BP->WidgetTree, TEXT("HBox_CraftCount"));
	Detail->AddChildToVerticalBox(CountRow);
	CountRow->AddChildToHorizontalBox(MakeText(BP->WidgetTree, TEXT("Text_CraftCountLabel"), FText::FromString(TEXT("제작 수량:")), 13));
	USlider* Slider = ConstructNamed<USlider>(BP->WidgetTree, TEXT("Slider_CraftCount"));
	Slider->SetMinValue(1.0f);
	Slider->SetMaxValue(1.0f);
	Slider->SetStepSize(1.0f);
	if (UHorizontalBoxSlot* Slot = CountRow->AddChildToHorizontalBox(Slider))
	{
		Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Slot->SetPadding(FMargin(8.0f, 0.0f));
	}
	CountRow->AddChildToHorizontalBox(MakeText(BP->WidgetTree, TEXT("Text_CraftCount"), FText::FromString(TEXT("1")), 13));

	UButton* CraftButton = MakeButtonWithText(BP->WidgetTree, TEXT("Button_Craft"), TEXT("Text_CraftButton"), FText::FromString(TEXT("제작하기")));
	Detail->AddChildToVerticalBox(CraftButton);

	if (UCraftPanelWidget* CDO = Cast<UCraftPanelWidget>(BP->GeneratedClass ? BP->GeneratedClass->GetDefaultObject() : nullptr))
	{
		CDO->RecipeEntryWidgetClass = LoadClass<UCraftRecipeEntryWidget>(nullptr, TEXT("/Game/Retrieve/UI/Craft/WBP_CraftRecipeEntry.WBP_CraftRecipeEntry_C"));
		CDO->MaterialRowWidgetClass = LoadClass<UCraftMaterialRowWidget>(nullptr, TEXT("/Game/Retrieve/UI/Craft/WBP_CraftMaterialRow.WBP_CraftMaterialRow_C"));
		CDO->CraftRecipeTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Retrieve/Data/Items/DT_CraftRecipe.DT_CraftRecipe"));
		CDO->ItemIconTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Retrieve/Data/Items/DT_ItemIcon.DT_ItemIcon"));
		CDO->MaterialItemTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Retrieve/Data/Items/DT_MaterialItem.DT_MaterialItem"));
	}

	CompileAndDirty(BP);
}

void ConfigureBurstPopup()
{
	UWidgetBlueprint* BP = CreateWidgetBlueprintIfMissing(
		TEXT("/Game/Retrieve/UI/HUD"),
		TEXT("WBP_BurstSkillPopup"),
		UUserWidget::StaticClass());
	if (!BP || !BP->WidgetTree)
	{
		return;
	}

	BP->WidgetTree->Modify();
	UCanvasPanel* Root = ConstructNamed<UCanvasPanel>(BP->WidgetTree, TEXT("Root"));
	BP->WidgetTree->RootWidget = Root;
	UTextBlock* SkillName = MakeText(BP->WidgetTree, TEXT("Text_SkillName"), FText::FromString(TEXT("Burst Skill")), 40, FLinearColor(1.0f, 0.85f, 0.3f, 1.0f));
	SkillName->SetJustification(ETextJustify::Center);
	Root->AddChild(SkillName);
	SetCanvasSlot(SkillName, FAnchors(0.5f), FVector2D::ZeroVector, FVector2D(500.0f, 80.0f), FVector2D(0.5f, 0.5f));
	CompileAndDirty(BP);
}


void ConfigureInteractionTypePromptWidgets()
{
	UClass* PromptClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/Retrieve/UI/Interaction/WBP_InteractionPrompt.WBP_InteractionPrompt_C"));
	if (!PromptClass)
	{
		return;
	}

	URetrieveInteractionPresetProfileAsset* Profile = LoadObject<URetrieveInteractionPresetProfileAsset>(
		nullptr,
		TEXT("/Game/Retrieve/Data/Interaction/DA_InteractionPresets_Default.DA_InteractionPresets_Default"));
	if (!Profile)
	{
		return;
	}

	Profile->Modify();
	for (FRetrieveInteractionPresetData& Preset : Profile->Presets)
	{
		Preset.WidgetClassOverride = PromptClass;
	}
	Profile->MarkPackageDirty();
}

void AddTextToNormalMonsterHealthBar()
{
	UWidgetBlueprint* BP = CreateWidgetBlueprintIfMissing(
		TEXT("/Game/Retrieve/UI"),
		TEXT("WBP_NormalMonsterHealthBar"),
		URetrieveNormalMonsterHealthBarWidget::StaticClass());
	if (!BP || !BP->WidgetTree)
	{
		return;
	}

	if (!BP->WidgetTree->RootWidget)
	{
		UCanvasPanel* Root = ConstructNamed<UCanvasPanel>(BP->WidgetTree, TEXT("Root"));
		BP->WidgetTree->RootWidget = Root;

		UProgressBar* HPBar = ConstructNamed<UProgressBar>(BP->WidgetTree, TEXT("HPBar"));
		Root->AddChild(HPBar);
		SetCanvasSlot(HPBar, FAnchors(0.0f, 0.5f, 1.0f, 0.5f), FVector2D(0.0f, 0.0f), FVector2D(0.0f, 16.0f), FVector2D(0.0f, 0.5f), 1);
	}

	UPanelWidget* RootPanel = Cast<UPanelWidget>(BP->WidgetTree->RootWidget);
	if (!RootPanel)
	{
		return;
	}

	if (!BP->WidgetTree->FindWidget(TEXT("Text_MonsterName")))
	{
		UTextBlock* Name = MakeText(BP->WidgetTree, TEXT("Text_MonsterName"), FText::FromString(TEXT("Monster")), 14);
		RootPanel->AddChild(Name);
		if (Cast<UCanvasPanel>(RootPanel))
		{
			SetCanvasSlot(Name, FAnchors(0.5f, 0.0f), FVector2D(0.0f, -18.0f), FVector2D(220.0f, 18.0f), FVector2D(0.5f, 0.0f), 2);
		}
	}

	if (!BP->WidgetTree->FindWidget(TEXT("Text_HPValue")))
	{
		UTextBlock* HP = MakeText(BP->WidgetTree, TEXT("Text_HPValue"), FText::FromString(TEXT("0 / 0")), 12);
		RootPanel->AddChild(HP);
		if (Cast<UCanvasPanel>(RootPanel))
		{
			SetCanvasSlot(HP, FAnchors(1.0f, 0.5f), FVector2D(-4.0f, 0.0f), FVector2D(100.0f, 18.0f), FVector2D(1.0f, 0.5f), 2);
		}
	}

	CompileAndDirty(BP);
}

void AddBuffStackCount()
{
	UWidgetBlueprint* BP = LoadWidgetBlueprint(TEXT("/Game/Retrieve/UI/HUD/WBP_BuffSlot.WBP_BuffSlot"));
	if (!BP || !BP->WidgetTree || BP->WidgetTree->FindWidget(TEXT("TXT_StackCount")))
	{
		return;
	}

	UPanelWidget* Content = Cast<UPanelWidget>(BP->WidgetTree->FindWidget(TEXT("Content")));
	if (!Content)
	{
		Content = Cast<UPanelWidget>(BP->WidgetTree->RootWidget);
	}
	if (!Content)
	{
		return;
	}

	UTextBlock* Stack = MakeText(BP->WidgetTree, TEXT("TXT_StackCount"), FText::FromString(TEXT("×1")), 12, FLinearColor(1.0f, 0.9f, 0.0f, 1.0f));
	Stack->SetJustification(ETextJustify::Right);
	Stack->SetVisibility(ESlateVisibility::Collapsed);
	Content->AddChild(Stack);
	if (Cast<UCanvasPanel>(Content))
	{
		SetCanvasSlot(Stack, FAnchors(1.0f, 1.0f), FVector2D(-2.0f, -2.0f), FVector2D(32.0f, 14.0f), FVector2D(1.0f, 1.0f), 5);
	}
	CompileAndDirty(BP);
}

void AddBurstPopupToHud()
{
	UWidgetBlueprint* BP = LoadWidgetBlueprint(TEXT("/Game/Retrieve/UI/WBP_HUD.WBP_HUD"));
	UClass* PopupClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/Retrieve/UI/HUD/WBP_BurstSkillPopup.WBP_BurstSkillPopup_C"));
	if (!BP || !BP->WidgetTree || !PopupClass || BP->WidgetTree->FindWidget(TEXT("WBP_BurstSkillPopup")))
	{
		return;
	}

	UCanvasPanel* Root = Cast<UCanvasPanel>(BP->WidgetTree->RootWidget);
	if (!Root)
	{
		return;
	}

	UWidget* Popup = BP->WidgetTree->ConstructWidget<UWidget>(PopupClass, TEXT("WBP_BurstSkillPopup"));
	Popup->SetVisibility(ESlateVisibility::Collapsed);
	Root->AddChild(Popup);
	SetCanvasSlot(Popup, FAnchors(0.5f), FVector2D(0.0f, -80.0f), FVector2D(500.0f, 120.0f), FVector2D(0.5f, 0.5f), 10);
	CompileAndDirty(BP);
}

void RemoveBurstPopupFromHud()
{
	UWidgetBlueprint* BP = LoadWidgetBlueprint(TEXT("/Game/Retrieve/UI/WBP_HUD.WBP_HUD"));
	if (!BP || !BP->WidgetTree)
	{
		return;
	}

	BP->Modify();
	BP->WidgetTree->Modify();
	BP->WidgetVariableNameToGuidMap.Remove(TEXT("WBP_BurstSkillPopup"));

	UWidget* Popup = BP->WidgetTree->FindWidget(TEXT("WBP_BurstSkillPopup"));
	if (!Popup)
	{
		CompileAndDirty(BP);
		return;
	}

	Popup->Modify();
	Popup->bIsVariable = false;
	BP->WidgetTree->RemoveWidget(Popup);
	BP->WidgetVariableNameToGuidMap.Remove(TEXT("WBP_BurstSkillPopup"));
	CompileAndDirty(BP);
}

void ConfigureElementGaugeSkillIcons()
{
	UWidgetBlueprint* BP = LoadWidgetBlueprint(TEXT("/Game/Retrieve/UI/HUD/WBP_ElementGauge.WBP_ElementGauge"));
	if (!BP || !BP->WidgetTree || !BP->WidgetTree->RootWidget)
	{
		return;
	}

	UPanelWidget* RootPanel = Cast<UPanelWidget>(BP->WidgetTree->RootWidget);
	if (!RootPanel)
	{
		return;
	}

	auto EnsureIcon = [BP, RootPanel](FName SizeBoxName, FName ImageName, const FVector2D& CanvasPosition)
	{
		constexpr float IconSize = 96.0f;

		USizeBox* IconBox = Cast<USizeBox>(BP->WidgetTree->FindWidget(SizeBoxName));
		if (!IconBox)
		{
			IconBox = ConstructNamed<USizeBox>(BP->WidgetTree, SizeBoxName);
			RootPanel->AddChild(IconBox);
		}
		IconBox->SetWidthOverride(IconSize);
		IconBox->SetHeightOverride(IconSize);
		IconBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		UImage* Icon = Cast<UImage>(BP->WidgetTree->FindWidget(ImageName));
		if (!Icon)
		{
			Icon = ConstructNamed<UImage>(BP->WidgetTree, ImageName);
			Icon->SetVisibility(ESlateVisibility::Collapsed);
			IconBox->ClearChildren();
			IconBox->AddChild(Icon);
		}
		Icon->SetVisibility(ESlateVisibility::Collapsed);
		Icon->SetDesiredSizeOverride(FVector2D(IconSize, IconSize));

		if (USizeBoxSlot* SizeSlot = Cast<USizeBoxSlot>(Icon->Slot))
		{
			SizeSlot->SetHorizontalAlignment(HAlign_Fill);
			SizeSlot->SetVerticalAlignment(VAlign_Fill);
			SizeSlot->SetPadding(FMargin(0.0f));
		}

		if (Cast<UCanvasPanel>(RootPanel))
		{
			SetCanvasSlot(IconBox, FAnchors(0.5f), CanvasPosition, FVector2D(IconSize, IconSize), FVector2D(0.5f, 0.5f), 6);
		}
	};

	EnsureIcon(TEXT("SizeBox_AbsorbSkillIcon"), TEXT("Image_AbsorbSkillIcon"), FVector2D(-238.0f, 0.0f));
	EnsureIcon(TEXT("SizeBox_BurstSkillIcon"), TEXT("Image_BurstSkillIcon"), FVector2D(238.0f, 0.0f));

	CompileAndDirty(BP);
}

void ConfigureBuffDefinitionIcons()
{
	struct FBuffIconSpec
	{
		const TCHAR* RowName;
		const TCHAR* FileName;
	};

	const FString SourceDir = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectContentDir() / TEXT("Retrieve/UI/Icons/Buff/Generated"));
	const FString DestinationPath = TEXT("/Game/Retrieve/UI/Icons/Buff/Generated");

	const FBuffIconSpec Specs[] = {
		{ TEXT("UI.Buff.Item.FireBoost"), TEXT("T_UI_Buff_Item_FireBoost.png") },
		{ TEXT("UI.Buff.Item.WaterBoost"), TEXT("T_UI_Buff_Item_WaterBoost.png") },
		{ TEXT("UI.Buff.Item.WindBoost"), TEXT("T_UI_Buff_Item_WindBoost.png") },
		{ TEXT("UI.Buff.Burst.FireSlash"), TEXT("T_UI_Buff_Burst_FireSlash.png") },
		{ TEXT("UI.Buff.Burst.WaterVortex"), TEXT("T_UI_Buff_Burst_WaterVortex.png") },
		{ TEXT("UI.Buff.Burst.WindSlash"), TEXT("T_UI_Buff_Burst_WindSlash.png") },
		{ TEXT("UI.Buff.Absorb.Fire"), TEXT("T_UI_Buff_Absorb_Fire.png") },
		{ TEXT("UI.Buff.Absorb.Water"), TEXT("T_UI_Buff_Absorb_Water.png") },
		{ TEXT("UI.Buff.Absorb.Wind"), TEXT("T_UI_Buff_Absorb_Wind.png") },
	};

	TArray<FString> FilesToImport;
	for (const FBuffIconSpec& Spec : Specs)
	{
		const FString TextureName = FPaths::GetBaseFilename(Spec.FileName);
		const FString TexturePath = DestinationPath / TextureName + TEXT(".") + TextureName;
		if (!LoadObject<UTexture2D>(nullptr, *TexturePath))
		{
			const FString SourceFile = SourceDir / Spec.FileName;
			if (FPaths::FileExists(SourceFile))
			{
				FilesToImport.Add(SourceFile);
			}
		}
	}

	if (!FilesToImport.IsEmpty())
	{
		AssetTools().Get().ImportAssets(FilesToImport, DestinationPath);
	}

	UDataTable* BuffTable = LoadObject<UDataTable>(
		nullptr,
		TEXT("/Game/Retrieve/Data/Skill/DT_BuffDefinitions.DT_BuffDefinitions"));
	if (!BuffTable)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Retrieve|EditorWorkGuide0612] DT_BuffDefinitions not found."));
		return;
	}

	BuffTable->Modify();
	for (const FBuffIconSpec& Spec : Specs)
	{
		FRetrieveBuffUIRow* Row = BuffTable->FindRow<FRetrieveBuffUIRow>(
			FName(Spec.RowName),
			TEXT("ConfigureBuffDefinitionIcons"));
		if (!Row)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Retrieve|EditorWorkGuide0612] Buff row not found: %s"), Spec.RowName);
			continue;
		}

		const FString TextureName = FPaths::GetBaseFilename(Spec.FileName);
		const FString TexturePath = DestinationPath / TextureName + TEXT(".") + TextureName;
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *TexturePath);
		if (!Texture)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Retrieve|EditorWorkGuide0612] Buff icon texture not found: %s"), *TexturePath);
			continue;
		}

		Row->Icon = TSoftObjectPtr<UTexture2D>(Texture);
	}

	BuffTable->MarkPackageDirty();
}

void ReplaceBonfireCraftPanel()
{
	UWidgetBlueprint* BP = LoadWidgetBlueprint(TEXT("/Game/Retrieve/UI/Bonfire/WBP_BonfireMenu.WBP_BonfireMenu"));
	UClass* CraftPanelClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/Retrieve/UI/Craft/WBP_CraftPanel.WBP_CraftPanel_C"));
	if (!BP || !BP->WidgetTree || !CraftPanelClass)
	{
		return;
	}

	UVerticalBox* Panel = Cast<UVerticalBox>(BP->WidgetTree->FindWidget(TEXT("Panel_Craft")));
	if (!Panel)
	{
		return;
	}

	if (!BP->WidgetTree->FindWidget(TEXT("HBox_SearchRow")))
	{
		UHorizontalBox* SearchRow = ConstructNamed<UHorizontalBox>(BP->WidgetTree, TEXT("HBox_SearchRow"));
		SearchRow->SetVisibility(ESlateVisibility::Collapsed);
		Panel->AddChildToVerticalBox(SearchRow);
	}

	UHorizontalBox* SearchRow = Cast<UHorizontalBox>(BP->WidgetTree->FindWidget(TEXT("HBox_SearchRow")));
	if (SearchRow && !BP->WidgetTree->FindWidget(TEXT("EditableText_Search")))
	{
		UEditableTextBox* SearchText = ConstructNamed<UEditableTextBox>(BP->WidgetTree, TEXT("EditableText_Search"));
		SearchText->SetVisibility(ESlateVisibility::Collapsed);
		SearchRow->AddChildToHorizontalBox(SearchText);
	}
	if (SearchRow && !BP->WidgetTree->FindWidget(TEXT("Btn_Search")))
	{
		UButton* SearchButton = MakeButtonWithText(BP->WidgetTree, TEXT("Btn_Search"), TEXT("Text_SearchButton"), FText::FromString(TEXT("검색")));
		SearchButton->SetVisibility(ESlateVisibility::Collapsed);
		SearchRow->AddChildToHorizontalBox(SearchButton);
	}

	if (!BP->WidgetTree->FindWidget(TEXT("SizeBox_0")))
	{
		USizeBox* RecipeSizeBox = ConstructNamed<USizeBox>(BP->WidgetTree, TEXT("SizeBox_0"));
		RecipeSizeBox->SetVisibility(ESlateVisibility::Collapsed);
		Panel->AddChildToVerticalBox(RecipeSizeBox);
	}

	USizeBox* RecipeSizeBox = Cast<USizeBox>(BP->WidgetTree->FindWidget(TEXT("SizeBox_0")));
	if (RecipeSizeBox && !BP->WidgetTree->FindWidget(TEXT("ScrollBox_Recipes")))
	{
		RecipeSizeBox->ClearChildren();
		UScrollBox* RecipeList = ConstructNamed<UScrollBox>(BP->WidgetTree, TEXT("ScrollBox_Recipes"));
		RecipeList->SetVisibility(ESlateVisibility::Collapsed);
		RecipeSizeBox->AddChild(RecipeList);
	}

	if (!BP->WidgetTree->FindWidget(TEXT("Text_Materials")))
	{
		UTextBlock* Materials = MakeText(BP->WidgetTree, TEXT("Text_Materials"), FText::GetEmpty(), 12);
		Materials->SetVisibility(ESlateVisibility::Collapsed);
		Panel->AddChildToVerticalBox(Materials);
	}

	if (!BP->WidgetTree->FindWidget(TEXT("Button_Craft")))
	{
		UButton* LegacyCraftButton = MakeButtonWithText(BP->WidgetTree, TEXT("Button_Craft"), TEXT("Text_LegacyCraftButton"), FText::FromString(TEXT("제작")));
		LegacyCraftButton->SetVisibility(ESlateVisibility::Collapsed);
		Panel->AddChildToVerticalBox(LegacyCraftButton);
	}

	if (UWidget* ExistingCraftPanel = BP->WidgetTree->FindWidget(TEXT("WBP_CraftPanel")))
	{
		if (ExistingCraftPanel->GetClass()->IsChildOf(CraftPanelClass))
		{
			ExistingCraftPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			if (UVerticalBoxSlot* ExistingSlot = Cast<UVerticalBoxSlot>(ExistingCraftPanel->Slot))
			{
				ExistingSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			}
			CompileAndDirty(BP);
			return;
		}

		BP->WidgetTree->RemoveWidget(ExistingCraftPanel);
	}

	UWidget* CraftPanel = BP->WidgetTree->ConstructWidget<UWidget>(CraftPanelClass, TEXT("WBP_CraftPanel"));
	CraftPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	if (UVerticalBoxSlot* Slot = Panel->AddChildToVerticalBox(CraftPanel))
	{
		Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}
	CompileAndDirty(BP);
}

#endif

void SetCurveKeys(UCurveFloat* Curve, const TArray<TPair<float, float>>& Keys)
{
	if (!Curve)
	{
		return;
	}

	Curve->Modify();
	Curve->FloatCurve.Reset();

	for (const TPair<float, float>& Key : Keys)
	{
		const FKeyHandle Handle = Curve->FloatCurve.AddKey(Key.Key, Key.Value);
		Curve->FloatCurve.SetKeyInterpMode(Handle, ERichCurveInterpMode::RCIM_Cubic);
	}

	Curve->MarkPackageDirty();
}

FRetrieveUIVFXPreset MakePreset(
	FGameplayTag EffectTag,
	UCurveFloat* Curve,
	float Duration,
	FVector2D StartTranslation,
	FVector2D EndTranslation,
	FVector2D StartScale,
	FVector2D EndScale,
	float StartOpacity,
	float EndOpacity)
{
	FRetrieveUIVFXPreset Preset;
	Preset.EffectTag = EffectTag;
	Preset.EffectName = EffectTag.GetTagName();
	Preset.Curve = Curve;
	Preset.Duration = Duration;
	Preset.StartTranslation = StartTranslation;
	Preset.EndTranslation = EndTranslation;
	Preset.StartScale = StartScale;
	Preset.EndScale = EndScale;
	Preset.StartOpacity = StartOpacity;
	Preset.EndOpacity = EndOpacity;
	return Preset;
}
}

bool URetrieveUIVFXEditorUtility::ConfigureRecommendedUIVFXAssets()
{
	URetrieveUIVFXProfile* Profile = LoadObject<URetrieveUIVFXProfile>(
		nullptr,
		TEXT("/Game/Retrieve/UI/VFX/DA_UIVFX_Default.DA_UIVFX_Default"));
	if (!Profile)
	{
		UE_LOG(LogTemp, Error, TEXT("[Retrieve|UIVFX] DA_UIVFX_Default was not found."));
		return false;
	}

	UCurveFloat* PanelIn = LoadCurve(TEXT("/Game/Retrieve/UI/VFX/Curves/C_UI_PanelSlide_In.C_UI_PanelSlide_In"));
	UCurveFloat* PanelOut = LoadCurve(TEXT("/Game/Retrieve/UI/VFX/Curves/C_UI_PanelSlide_Out.C_UI_PanelSlide_Out"));
	UCurveFloat* GaugePulse = LoadCurve(TEXT("/Game/Retrieve/UI/VFX/Curves/C_UI_GaugePulse.C_UI_GaugePulse"));
	UCurveFloat* IconFlash = LoadCurve(TEXT("/Game/Retrieve/UI/VFX/Curves/C_UI_IconFlash.C_UI_IconFlash"));
	UCurveFloat* ButtonHover = LoadCurve(TEXT("/Game/Retrieve/UI/VFX/Curves/C_UI_ButtonHover.C_UI_ButtonHover"));
	UCurveFloat* ButtonUnhover = LoadCurve(TEXT("/Game/Retrieve/UI/VFX/Curves/C_UI_ButtonUnhover.C_UI_ButtonUnhover"));
	UCurveFloat* ButtonPress = LoadCurve(TEXT("/Game/Retrieve/UI/VFX/Curves/C_UI_ButtonPress.C_UI_ButtonPress"));
	UCurveFloat* ButtonRelease = LoadCurve(TEXT("/Game/Retrieve/UI/VFX/Curves/C_UI_ButtonRelease.C_UI_ButtonRelease"));
	UCurveFloat* TabSwitch = LoadCurve(TEXT("/Game/Retrieve/UI/VFX/Curves/C_UI_TabSwitch.C_UI_TabSwitch"));

	SetCurveKeys(PanelIn, {
		{0.00f, 0.00f},
		{0.28f, 0.82f},
		{0.58f, 1.04f},
		{1.00f, 1.00f},
	});
	SetCurveKeys(PanelOut, {
		{0.00f, 0.00f},
		{0.30f, 0.48f},
		{0.72f, 0.92f},
		{1.00f, 1.00f},
	});
	SetCurveKeys(GaugePulse, {
		{0.00f, 0.00f},
		{0.18f, 1.22f},
		{0.46f, 0.90f},
		{0.74f, 1.06f},
		{1.00f, 1.00f},
	});
	SetCurveKeys(IconFlash, {
		{0.00f, 0.00f},
		{0.18f, 1.18f},
		{0.48f, 0.96f},
		{1.00f, 1.00f},
	});
	SetCurveKeys(ButtonHover, {
		{0.00f, 0.00f},
		{0.55f, 0.90f},
		{1.00f, 1.00f},
	});
	SetCurveKeys(ButtonUnhover, {
		{0.00f, 0.00f},
		{0.55f, 0.90f},
		{1.00f, 1.00f},
	});
	SetCurveKeys(ButtonPress, {
		{0.00f, 0.00f},
		{1.00f, 1.00f},
	});
	SetCurveKeys(ButtonRelease, {
		{0.00f, 0.00f},
		{0.40f, 1.08f},
		{1.00f, 1.00f},
	});
	SetCurveKeys(TabSwitch, {
		{0.00f, 0.00f},
		{0.35f, 0.76f},
		{1.00f, 1.00f},
	});

	Profile->Modify();
	Profile->Presets.Reset();
	Profile->Presets.Add(MakePreset(
		RetrieveGameplayTags::UI_VFX_Panel_Open,
		PanelIn,
		0.28f,
		FVector2D(0.0f, 42.0f),
		FVector2D::ZeroVector,
		FVector2D(0.97f, 0.97f),
		FVector2D(1.0f, 1.0f),
		0.0f,
		1.0f));
	Profile->Presets.Add(MakePreset(
		RetrieveGameplayTags::UI_VFX_Panel_Close,
		PanelOut,
		0.20f,
		FVector2D::ZeroVector,
		FVector2D(0.0f, 32.0f),
		FVector2D(1.0f, 1.0f),
		FVector2D(0.98f, 0.98f),
		1.0f,
		0.0f));
	Profile->Presets.Add(MakePreset(
		RetrieveGameplayTags::UI_VFX_Gauge_FullPulse,
		GaugePulse,
		0.34f,
		FVector2D::ZeroVector,
		FVector2D::ZeroVector,
		FVector2D(1.0f, 1.0f),
		FVector2D(1.07f, 1.07f),
		1.0f,
		1.0f));
	Profile->Presets.Add(MakePreset(
		RetrieveGameplayTags::UI_VFX_Icon_ItemAdded,
		IconFlash,
		0.30f,
		FVector2D(0.0f, 12.0f),
		FVector2D::ZeroVector,
		FVector2D(0.90f, 0.90f),
		FVector2D(1.0f, 1.0f),
		0.0f,
		1.0f));
	Profile->Presets.Add(MakePreset(
		RetrieveGameplayTags::UI_VFX_Button_Hover,
		ButtonHover,
		0.12f,
		FVector2D::ZeroVector,
		FVector2D(0.0f, -2.0f),
		FVector2D(1.0f, 1.0f),
		FVector2D(1.035f, 1.035f),
		1.0f,
		1.0f));
	Profile->Presets.Add(MakePreset(
		RetrieveGameplayTags::UI_VFX_Button_Unhover,
		ButtonUnhover,
		0.12f,
		FVector2D(0.0f, -2.0f),
		FVector2D::ZeroVector,
		FVector2D(1.035f, 1.035f),
		FVector2D(1.0f, 1.0f),
		1.0f,
		1.0f));
	Profile->Presets.Add(MakePreset(
		RetrieveGameplayTags::UI_VFX_Button_Press,
		ButtonPress,
		0.07f,
		FVector2D::ZeroVector,
		FVector2D(0.0f, 1.0f),
		FVector2D(1.035f, 1.035f),
		FVector2D(0.965f, 0.965f),
		1.0f,
		1.0f));
	Profile->Presets.Add(MakePreset(
		RetrieveGameplayTags::UI_VFX_Button_Release,
		ButtonRelease,
		0.12f,
		FVector2D(0.0f, 1.0f),
		FVector2D::ZeroVector,
		FVector2D(0.965f, 0.965f),
		FVector2D(1.02f, 1.02f),
		1.0f,
		1.0f));
	Profile->Presets.Add(MakePreset(
		RetrieveGameplayTags::UI_VFX_Tab_Switch,
		TabSwitch,
		0.18f,
		FVector2D(10.0f, 0.0f),
		FVector2D::ZeroVector,
		FVector2D(0.995f, 0.995f),
		FVector2D(1.0f, 1.0f),
		0.65f,
		1.0f));

	Profile->MarkPackageDirty();
	UE_LOG(LogTemp, Log, TEXT("[Retrieve|UIVFX] Recommended UI VFX profile and curves configured."));
	return true;
}

bool URetrieveUIVFXEditorUtility::ConfigureEditorWorkGuide0612Assets()
{
#if WITH_EDITOR
	ConfigureCraftMaterialRow();
	ConfigureCraftRecipeEntry();
	ConfigureCraftPanel();
	ConfigureInteractionTypePromptWidgets();

	AddTextToNormalMonsterHealthBar();
	AddBuffStackCount();
	RemoveBurstPopupFromHud();
	ConfigureElementGaugeSkillIcons();
	ConfigureBuffDefinitionIcons();
	ReplaceBonfireCraftPanel();

	UE_LOG(LogTemp, Log, TEXT("[Retrieve|EditorWorkGuide0612] Monolith editor guide assets configured."));
	return true;
#else
	UE_LOG(LogTemp, Warning, TEXT("[Retrieve|EditorWorkGuide0612] Editor-only asset configuration is unavailable in non-editor builds."));
	return false;
#endif
}

bool URetrieveUIVFXEditorUtility::ConfigureCraftUIAssets()
{
#if WITH_EDITOR
	ConfigureCraftMaterialRow();
	ConfigureCraftRecipeEntry();
	ConfigureCraftPanel();
	UE_LOG(LogTemp, Log, TEXT("[Retrieve|CraftUI] Craft UI assets configured."));
	return true;
#else
	UE_LOG(LogTemp, Warning, TEXT("[Retrieve|CraftUI] Editor-only craft UI configuration is unavailable in non-editor builds."));
	return false;
#endif
}

bool URetrieveUIVFXEditorUtility::ReplaceBonfireCraftPanelOnly()
{
#if WITH_EDITOR
	ReplaceBonfireCraftPanel();
	UE_LOG(LogTemp, Log, TEXT("[Retrieve|CraftUI] Bonfire craft panel reference configured."));
	return true;
#else
	UE_LOG(LogTemp, Warning, TEXT("[Retrieve|CraftUI] Editor-only bonfire craft panel replacement is unavailable in non-editor builds."));
	return false;
#endif
}

bool URetrieveUIVFXEditorUtility::FixElementGaugeSkillIconFrames()
{
#if WITH_EDITOR
	ConfigureElementGaugeSkillIcons();
	UE_LOG(LogTemp, Log, TEXT("[Retrieve|ElementGauge] Skill icon frames configured."));
	return true;
#else
	UE_LOG(LogTemp, Warning, TEXT("[Retrieve|ElementGauge] Editor-only icon frame configuration is unavailable in non-editor builds."));
	return false;
#endif
}
