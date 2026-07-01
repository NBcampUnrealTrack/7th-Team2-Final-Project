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
#include "Editor.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UI/Bonfire/BonfireMenuWidget.h"
#include "UI/Craft/CraftMaterialRowWidget.h"
#include "UI/Craft/CraftPanelWidget.h"
#include "UI/Craft/CraftRecipeEntryWidget.h"
#include "UI/HUD/RetrieveNormalMonsterHealthBarWidget.h"
#include "UI/HUD/RetrieveElementSkillWidget.h"
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

template <typename T>
T* FindOrConstructNamed(UWidgetTree* Tree, FName Name)
{
	if (!Tree)
	{
		return nullptr;
	}

	if (UWidget* Existing = Tree->FindWidget(Name))
	{
		Existing->bIsVariable = true;
		return Cast<T>(Existing);
	}

	return ConstructNamed<T>(Tree, Name);
}

void AddChildIfNeeded(UPanelWidget* Parent, UWidget* Child)
{
	if (Parent && Child && !Child->Slot)
	{
		Parent->AddChild(Child);
	}
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

		const FName WidgetName = Widget->GetFName();
		if (WidgetName.ToString().StartsWith(TEXT("Retired_")))
		{
			Widget->bIsVariable = false;
			continue;
		}

		SeenNames.Add(WidgetName);
		Widget->bIsVariable = true;
		if (!BP->WidgetVariableNameToGuidMap.Contains(WidgetName))
		{
			BP->WidgetVariableNameToGuidMap.Add(WidgetName, FGuid::NewGuid());
		}
	}

	for (auto It = BP->WidgetVariableNameToGuidMap.CreateIterator(); It; ++It)
	{
		if (It.Key().ToString().StartsWith(TEXT("Retired_")) || !SeenNames.Contains(It.Key()))
		{
			It.RemoveCurrent();
		}
	}

	BP->NewVariables.RemoveAll([](const FBPVariableDescription& Variable)
	{
		return Variable.VarName.ToString().StartsWith(TEXT("Retired_"));
	});
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
			const FString RetiredName = FString::Printf(
				TEXT("Retired_%s_%s"),
				*Widget->GetName(),
				*FGuid::NewGuid().ToString(EGuidFormats::Digits));
			Widget->Rename(*RetiredName, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
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

void ConfigureMonsterHealthBarLayoutAsset()
{
	UWidgetBlueprint* BP = CreateWidgetBlueprintIfMissing(
		TEXT("/Game/Retrieve/UI"),
		TEXT("WBP_MonsterHealthBar"),
		URetrieveNormalMonsterHealthBarWidget::StaticClass());
	if (!BP || !BP->WidgetTree)
	{
		return;
	}

	UTexture2D* EpicFrameTexture = LoadObject<UTexture2D>(
		nullptr,
		TEXT("/Game/External/UIFantasyWarriorHUD/Textures/FantasyWarrior/T_FantasyWarrior_Bar_Horizontal08.T_FantasyWarrior_Bar_Horizontal08"));

	BP->Modify();
	ResetWidgetTree(BP);

	UCanvasPanel* Root = FindOrConstructNamed<UCanvasPanel>(BP->WidgetTree, TEXT("Root"));
	if (!Root)
	{
		Root = ConstructNamed<UCanvasPanel>(BP->WidgetTree, TEXT("MonsterHealthBarRoot"));
	}
	BP->WidgetTree->RootWidget = Root;

	UBorder* Backplate = FindOrConstructNamed<UBorder>(BP->WidgetTree, TEXT("MonsterHP_BG_Backplate"));
	Backplate->SetBrushColor(FLinearColor(0.015f, 0.012f, 0.01f, 0.84f));
	AddChildIfNeeded(Root, Backplate);
	SetCanvasSlot(Backplate, FAnchors(0.5f, 0.5f, 0.5f, 0.5f), FVector2D(0.0f, 2.0f), FVector2D(252.0f, 42.0f), FVector2D(0.5f, 0.5f), 0);

	UWidget* Vignette = BP->WidgetTree->FindWidget(TEXT("FRA_Vignette"));
	if (!Vignette)
	{
		Vignette = ConstructNamed<UBorder>(BP->WidgetTree, TEXT("FRA_Vignette"));
		if (UBorder* VignetteBorder = Cast<UBorder>(Vignette))
		{
			VignetteBorder->SetBrushColor(FLinearColor(0.45f, 0.25f, 0.02f, 0.28f));
		}
	}
	if (Vignette)
	{
		Vignette->SetVisibility(ESlateVisibility::Collapsed);
	}
	AddChildIfNeeded(Root, Vignette);
	SetCanvasSlot(Vignette, FAnchors(0.5f, 0.5f, 0.5f, 0.5f), FVector2D(0.0f, 2.0f), FVector2D(264.0f, 52.0f), FVector2D(0.5f, 0.5f), 1);

	UImage* Frame = FindOrConstructNamed<UImage>(BP->WidgetTree, TEXT("FRA_Frame"));
	if (Frame)
	{
		if (EpicFrameTexture)
		{
			Frame->SetBrushFromTexture(EpicFrameTexture, true);
		}
		Frame->SetColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f));
		Frame->SetVisibility(ESlateVisibility::Collapsed);
	}
	AddChildIfNeeded(Root, Frame);
	SetCanvasSlot(Frame, FAnchors(0.5f, 0.5f, 0.5f, 0.5f), FVector2D(0.0f, 2.0f), FVector2D(284.0f, 58.0f), FVector2D(0.5f, 0.5f), 6);

	UBorder* HPTrack = FindOrConstructNamed<UBorder>(BP->WidgetTree, TEXT("MonsterHP_BG_HPTrack"));
	HPTrack->SetBrushColor(FLinearColor(0.055f, 0.018f, 0.018f, 0.96f));
	AddChildIfNeeded(Root, HPTrack);
	SetCanvasSlot(HPTrack, FAnchors(0.5f, 0.5f, 0.5f, 0.5f), FVector2D(0.0f, -1.0f), FVector2D(236.0f, 16.0f), FVector2D(0.5f, 0.5f), 3);

	UProgressBar* HPBar = FindOrConstructNamed<UProgressBar>(BP->WidgetTree, TEXT("HPBar"));
	if (!HPBar)
	{
		UE_LOG(LogTemp, Error, TEXT("[Retrieve|MonsterHealthBar] HPBar exists but is not a ProgressBar."));
		return;
	}
	HPBar->SetPercent(1.0f);
	HPBar->SetFillColorAndOpacity(FLinearColor(0.84f, 0.04f, 0.035f, 0.98f));
	AddChildIfNeeded(Root, HPBar);
	SetCanvasSlot(HPBar, FAnchors(0.5f, 0.5f, 0.5f, 0.5f), FVector2D(0.0f, -1.0f), FVector2D(228.0f, 10.0f), FVector2D(0.5f, 0.5f), 4);

	UBorder* GroggyTrack = FindOrConstructNamed<UBorder>(BP->WidgetTree, TEXT("MonsterHP_BG_GroggyTrack"));
	GroggyTrack->SetBrushColor(FLinearColor(0.08f, 0.055f, 0.012f, 0.98f));
	AddChildIfNeeded(Root, GroggyTrack);
	SetCanvasSlot(GroggyTrack, FAnchors(0.5f, 0.5f, 0.5f, 0.5f), FVector2D(0.0f, 13.0f), FVector2D(236.0f, 9.0f), FVector2D(0.5f, 0.5f), 3);

	UProgressBar* GroggyProgressBar = FindOrConstructNamed<UProgressBar>(BP->WidgetTree, TEXT("GroggyProgressBar"));
	if (!GroggyProgressBar)
	{
		UE_LOG(LogTemp, Error, TEXT("[Retrieve|MonsterHealthBar] GroggyProgressBar exists but is not a ProgressBar."));
		return;
	}
	GroggyProgressBar->SetPercent(0.0f);
	GroggyProgressBar->SetFillColorAndOpacity(FLinearColor(1.0f, 0.72f, 0.08f, 0.98f));
	GroggyProgressBar->SetVisibility(ESlateVisibility::Hidden);
	AddChildIfNeeded(Root, GroggyProgressBar);
	SetCanvasSlot(GroggyProgressBar, FAnchors(0.5f, 0.5f, 0.5f, 0.5f), FVector2D(0.0f, 13.0f), FVector2D(228.0f, 5.0f), FVector2D(0.5f, 0.5f), 4);

	UTextBlock* Name = FindOrConstructNamed<UTextBlock>(BP->WidgetTree, TEXT("Text_MonsterName"));
	Name->SetJustification(ETextJustify::Center);
	Name->SetText(FText::FromString(TEXT("Monster")));
	SetFontSize(Name, 13);
	Name->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.93f, 0.78f, 1.0f)));
	AddChildIfNeeded(Root, Name);
	SetCanvasSlot(Name, FAnchors(0.5f, 0.5f, 0.5f, 0.5f), FVector2D(0.0f, -21.0f), FVector2D(220.0f, 18.0f), FVector2D(0.5f, 0.5f), 5);

	UTextBlock* HPValue = FindOrConstructNamed<UTextBlock>(BP->WidgetTree, TEXT("Text_HPValue"));
	HPValue->SetJustification(ETextJustify::Right);
	HPValue->SetText(FText::FromString(TEXT("0 / 0")));
	SetFontSize(HPValue, 11);
	HPValue->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.93f, 0.86f, 0.96f)));
	AddChildIfNeeded(Root, HPValue);
	SetCanvasSlot(HPValue, FAnchors(0.5f, 0.5f, 0.5f, 0.5f), FVector2D(104.0f, -1.0f), FVector2D(96.0f, 14.0f), FVector2D(1.0f, 0.5f), 5);

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

void RemoveElementSkillIconsFromGauge()
{
	UWidgetBlueprint* BP = LoadWidgetBlueprint(TEXT("/Game/Retrieve/UI/HUD/WBP_ElementGauge.WBP_ElementGauge"));
	if (!BP || !BP->WidgetTree)
	{
		return;
	}

	BP->Modify();
	BP->WidgetTree->Modify();
	// 컨테이너를 먼저 제거하면 자식 Image도 트리에서 함께 분리된다.
	// GUID 맵은 CompileAndDirty의 EnsureWidgetVariableGuids가 최종 트리를 기준으로 정리한다.
	const FName WidgetNames[] = {
		TEXT("SizeBox_AbsorbSkillIcon"),
		TEXT("SizeBox_BurstSkillIcon"),
		TEXT("Image_AbsorbSkillIcon"),
		TEXT("Image_BurstSkillIcon")
	};
	for (const FName WidgetName : WidgetNames)
	{
		if (UWidget* Widget = BP->WidgetTree->FindWidget(WidgetName))
		{
			Widget->Modify();
			Widget->bIsVariable = false;
			BP->WidgetTree->RemoveWidget(Widget);
		}
	}
	CompileAndDirty(BP);
}

UWidgetBlueprint* ConfigureElementSkillPanel()
{
	UWidgetBlueprint* BP = CreateWidgetBlueprintIfMissing(
		TEXT("/Game/Retrieve/UI/HUD"),
		TEXT("WBP_ElementSkillPanel"),
		URetrieveElementSkillWidget::StaticClass());
	if (!BP || !BP->WidgetTree)
	{
		return nullptr;
	}

	UCanvasPanel* RootPanel = Cast<UCanvasPanel>(BP->WidgetTree->RootWidget);
	if (!RootPanel)
	{
		RootPanel = ConstructNamed<UCanvasPanel>(BP->WidgetTree, TEXT("Canvas_ElementSkills"));
		BP->WidgetTree->RootWidget = RootPanel;
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

		SetCanvasSlot(IconBox, FAnchors(0.5f), CanvasPosition, FVector2D(IconSize, IconSize), FVector2D(0.5f, 0.5f), 6);
	};

	EnsureIcon(TEXT("SizeBox_AbsorbSkillIcon"), TEXT("Image_AbsorbSkillIcon"), FVector2D(-238.0f, 0.0f));
	EnsureIcon(TEXT("SizeBox_BurstSkillIcon"), TEXT("Image_BurstSkillIcon"), FVector2D(238.0f, 0.0f));

	CompileAndDirty(BP);
	return BP;
}

void AddElementSkillPanelToHud()
{
	UWidgetBlueprint* HUD = LoadWidgetBlueprint(TEXT("/Game/Retrieve/UI/WBP_HUD.WBP_HUD"));
	UClass* SkillPanelClass = LoadClass<UUserWidget>(
		nullptr,
		TEXT("/Game/Retrieve/UI/HUD/WBP_ElementSkillPanel.WBP_ElementSkillPanel_C"));
	if (!HUD || !HUD->WidgetTree || !SkillPanelClass)
	{
		return;
	}

	UCanvasPanel* Root = Cast<UCanvasPanel>(HUD->WidgetTree->RootWidget);
	if (!Root)
	{
		return;
	}

	UWidget* SkillPanel = HUD->WidgetTree->FindWidget(TEXT("WBP_ElementSkillPanel"));
	if (!SkillPanel)
	{
		SkillPanel = HUD->WidgetTree->ConstructWidget<UWidget>(SkillPanelClass, TEXT("WBP_ElementSkillPanel"));
		Root->AddChild(SkillPanel);
	}
	SkillPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	FAnchors Anchors(0.5f);
	FVector2D Position = FVector2D::ZeroVector;
	FVector2D Size(572.0f, 96.0f);
	FVector2D Alignment(0.5f, 0.5f);
	int32 ZOrder = 1;
	if (UWidget* Gauge = HUD->WidgetTree->FindWidget(TEXT("WBP_ElementGauge")))
	{
		if (const UCanvasPanelSlot* GaugeSlot = Cast<UCanvasPanelSlot>(Gauge->Slot))
		{
			Anchors = GaugeSlot->GetAnchors();
			Position = GaugeSlot->GetPosition();
			Size.X = FMath::Max(Size.X, GaugeSlot->GetSize().X);
			Size.Y = FMath::Max(Size.Y, GaugeSlot->GetSize().Y);
			Alignment = GaugeSlot->GetAlignment();
			ZOrder = GaugeSlot->GetZOrder() + 1;
		}
	}
	SetCanvasSlot(SkillPanel, Anchors, Position, Size, Alignment, ZOrder);
	CompileAndDirty(HUD);
}

void SaveElementWidgetAssets(UWidgetBlueprint* SkillPanel)
{
	UEditorAssetSubsystem* AssetSubsystem = GEditor ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
	if (!AssetSubsystem)
	{
		return;
	}

	if (UWidgetBlueprint* Gauge = LoadWidgetBlueprint(TEXT("/Game/Retrieve/UI/HUD/WBP_ElementGauge.WBP_ElementGauge")))
	{
		AssetSubsystem->SaveLoadedAsset(Gauge, false);
	}
	if (SkillPanel)
	{
		AssetSubsystem->SaveLoadedAsset(SkillPanel, false);
	}
	if (UWidgetBlueprint* HUD = LoadWidgetBlueprint(TEXT("/Game/Retrieve/UI/WBP_HUD.WBP_HUD")))
	{
		AssetSubsystem->SaveLoadedAsset(HUD, false);
	}
}

void ConfigureSeparatedElementWidgets()
{
	RemoveElementSkillIconsFromGauge();
	UWidgetBlueprint* SkillPanel = ConfigureElementSkillPanel();
	AddElementSkillPanelToHud();
	SaveElementWidgetAssets(SkillPanel);
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
	ConfigureSeparatedElementWidgets();
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
	ConfigureSeparatedElementWidgets();
	UE_LOG(LogTemp, Log, TEXT("[Retrieve|ElementGauge] Gauge and skill panel separated."));
	return true;
#else
	UE_LOG(LogTemp, Warning, TEXT("[Retrieve|ElementGauge] Editor-only icon frame configuration is unavailable in non-editor builds."));
	return false;
#endif
}

bool URetrieveUIVFXEditorUtility::ConfigureMonsterHealthBarLayout()
{
#if WITH_EDITOR
	ConfigureMonsterHealthBarLayoutAsset();
	UE_LOG(LogTemp, Log, TEXT("[Retrieve|MonsterHealthBar] WBP_MonsterHealthBar layout configured."));
	return true;
#else
	UE_LOG(LogTemp, Warning, TEXT("[Retrieve|MonsterHealthBar] Editor-only monster health bar configuration is unavailable in non-editor builds."));
	return false;
#endif
}
