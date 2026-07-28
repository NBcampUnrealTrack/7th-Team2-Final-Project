#include "UI/HUD/RetrieveElementSkillWidget.h"

#include "Components/Image.h"
#include "Components/Player/WeaponComponent.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MVVMSubsystem.h"
#include "Player/RetrievePlayerController.h"
#include "UI/RetrieveElementUILibrary.h"
#include "UI/ViewModels/ElementGaugeViewModel.h"
#include "UI/ViewModels/HUDViewModel.h"
#include "UObject/ConstructorHelpers.h"
#include "View/MVVMView.h"

URetrieveElementSkillWidget::URetrieveElementSkillWidget()
{
	static ConstructorHelpers::FObjectFinder<UDataTable> BuffTableFinder(
		TEXT("/Game/Retrieve/Data/Skill/DT_BuffDefinitions.DT_BuffDefinitions"));
	if (BuffTableFinder.Succeeded())
	{
		BuffDefinitionTable = BuffTableFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UDataTable> SkillCombinationTableFinder(
		TEXT("/Game/Retrieve/Data/Skill/DT_SkillCombination.DT_SkillCombination"));
	if (SkillCombinationTableFinder.Succeeded())
	{
		SkillCombinationTable = SkillCombinationTableFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SkillIconMaterialFinder(
		TEXT("/Game/Retrieve/UI/Materials/M_UI_SkillIcon_Masked.M_UI_SkillIcon_Masked"));
	if (SkillIconMaterialFinder.Succeeded())
	{
		SkillIconMaskedMaterial = SkillIconMaterialFinder.Object;
	}
}

void URetrieveElementSkillWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindSources();
	UpdateSkillIcons();
}

void URetrieveElementSkillWidget::NativeDestruct()
{
	if (UElementGaugeViewModel* VM = BoundViewModel.Get())
	{
		VM->OnSlotsUpdated.RemoveDynamic(this, &ThisClass::HandleGaugeUpdated);
	}

	if (UWeaponComponent* Weapon = BoundWeaponComponent.Get())
	{
		Weapon->OnWeaponEquipped.RemoveDynamic(this, &ThisClass::HandleWeaponChanged);
		Weapon->OnWeaponUnequipped.RemoveDynamic(this, &ThisClass::HandleWeaponChanged);
	}

	BoundViewModel.Reset();
	BoundWeaponComponent.Reset();
	Super::NativeDestruct();
}

void URetrieveElementSkillWidget::NativeOnElementModeChanged(FGameplayTag NewElement)
{
	UpdateSkillIcons();
	Super::NativeOnElementModeChanged(NewElement);
}

void URetrieveElementSkillWidget::BindSources()
{
	ARetrievePlayerController* RPC = Cast<ARetrievePlayerController>(GetOwningPlayer());
	UHUDViewModel* HUDVM = RPC ? RPC->GetHUDViewModel() : nullptr;
	UElementGaugeViewModel* GaugeVM = HUDVM ? HUDVM->GetElementGauge() : nullptr;
	if (GaugeVM)
	{
		BoundViewModel = GaugeVM;
		GaugeVM->OnSlotsUpdated.RemoveDynamic(this, &ThisClass::HandleGaugeUpdated);
		GaugeVM->OnSlotsUpdated.AddDynamic(this, &ThisClass::HandleGaugeUpdated);

		if (UMVVMSubsystem* MVVM = GEngine ? GEngine->GetEngineSubsystem<UMVVMSubsystem>() : nullptr)
		{
			if (UMVVMView* View = MVVM->GetViewFromUserWidget(this))
			{
				View->SetViewModel(TEXT("ElementGauge"), GaugeVM);
				View->SetViewModel(TEXT("ElementGaugeViewModel"), GaugeVM);
			}
		}
	}

	APawn* Pawn = GetOwningPlayerPawn();
	UWeaponComponent* Weapon = Pawn ? Pawn->FindComponentByClass<UWeaponComponent>() : nullptr;
	if (Weapon)
	{
		BoundWeaponComponent = Weapon;
		Weapon->OnWeaponEquipped.RemoveDynamic(this, &ThisClass::HandleWeaponChanged);
		Weapon->OnWeaponUnequipped.RemoveDynamic(this, &ThisClass::HandleWeaponChanged);
		Weapon->OnWeaponEquipped.AddDynamic(this, &ThisClass::HandleWeaponChanged);
		Weapon->OnWeaponUnequipped.AddDynamic(this, &ThisClass::HandleWeaponChanged);
	}
}

void URetrieveElementSkillWidget::HandleGaugeUpdated()
{
	UpdateSkillIcons();
}

void URetrieveElementSkillWidget::HandleWeaponChanged(FName /*WeaponItemId*/)
{
	UpdateSkillIcons();
}

void URetrieveElementSkillWidget::UpdateSkillIcons()
{
	EnsureSkillIconTables();

	FRetrieveBuffUIRow AbsorbRow;
	const UElementGaugeViewModel* VM = BoundViewModel.Get();
	const FGameplayTag AbsorbElement = (VM && VM->GetSlot0IsFull())
		? VM->GetSlot0Element()
		: RetrieveGameplayTags::Element_None;
	const FGameplayTag AbsorbUITag = URetrieveElementUILibrary::ElementToAbsorbBuffUITag(AbsorbElement);
	const bool bHasAbsorbRow = URetrieveElementUILibrary::GetBuffUIRow(BuffDefinitionTable, AbsorbUITag, AbsorbRow);
	ApplySkillIcon(Image_AbsorbSkillIcon, AbsorbSkillIconMID, bHasAbsorbRow ? &AbsorbRow : nullptr, bHasAbsorbRow);

	FRetrieveBuffUIRow BurstRow;
	const bool bHasBurstRow = ResolveBurstBuffUIRow(BurstRow);
	ApplySkillIcon(Image_BurstSkillIcon, BurstSkillIconMID, bHasBurstRow ? &BurstRow : nullptr, bHasBurstRow);
}

void URetrieveElementSkillWidget::EnsureSkillIconTables()
{
	if (!BuffDefinitionTable)
	{
		BuffDefinitionTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Retrieve/Data/Skill/DT_BuffDefinitions.DT_BuffDefinitions"));
	}
	if (!SkillCombinationTable)
	{
		SkillCombinationTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Retrieve/Data/Skill/DT_SkillCombination.DT_SkillCombination"));
	}
}

bool URetrieveElementSkillWidget::ResolveBurstBuffUIRow(FRetrieveBuffUIRow& OutRow) const
{
	const UElementGaugeViewModel* VM = BoundViewModel.Get();
	if (!VM || !VM->GetIsGaugeFull())
	{
		return false;
	}

	FGameplayTag WeaponTypeTag;
	if (const UWeaponComponent* Weapon = BoundWeaponComponent.Get())
	{
		WeaponTypeTag = Weapon->GetWeaponDataRef().WeaponTypeTag;
	}

	FSkillCombination Combination;
	if (!URetrieveElementUILibrary::GetBurstCombinationByElement(
		SkillCombinationTable, WeaponTypeTag, VM->GetCurrentElement(), Combination))
	{
		return false;
	}

	return URetrieveElementUILibrary::GetBuffUIRow(BuffDefinitionTable, Combination.BurstUITag, OutRow);
}

void URetrieveElementSkillWidget::ApplySkillIcon(
	UImage* Image,
	TObjectPtr<UMaterialInstanceDynamic>& IconMID,
	const FRetrieveBuffUIRow* Row,
	bool bEnabled)
{
	if (!Image)
	{
		return;
	}

	UTexture2D* IconTexture = (Row && !Row->Icon.IsNull()) ? Row->Icon.LoadSynchronous() : nullptr;
	if (!IconTexture)
	{
		Image->SetVisibility(ESlateVisibility::Collapsed);
		Image->SetBrushFromTexture(nullptr, false);
		IconMID = nullptr;
		return;
	}

	if (!IconMID && SkillIconMaskedMaterial)
	{
		IconMID = UMaterialInstanceDynamic::Create(SkillIconMaskedMaterial, this);
	}

	if (IconMID)
	{
		Image->SetBrushFromMaterial(IconMID);
		Image->SetDesiredSizeOverride(FVector2D(96.0f, 96.0f));
		IconMID->SetTextureParameterValue(TEXT("IconTexture"), IconTexture);
		IconMID->SetTextureParameterValue(TEXT("Texture"), IconTexture);
		IconMID->SetTextureParameterValue(TEXT("SkillIcon"), IconTexture);
		IconMID->SetTextureParameterValue(TEXT("Icon"), IconTexture);
	}
	else
	{
		Image->SetBrushFromTexture(IconTexture, false);
	}

	Image->SetColorAndOpacity(Row->TintColor);
	Image->SetRenderOpacity(bEnabled ? 1.0f : 0.35f);
	Image->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}
