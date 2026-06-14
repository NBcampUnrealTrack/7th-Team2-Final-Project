#include "Commandlets/RetrieveMigrateInteractionPresetsCommandlet.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/RetrieveInteractionResponseComponent.h"
#include "Data/Interaction/RetrieveInteractionPresetAsset.h"
#include "Data/Interaction/RetrieveInteractionPresetProfileAsset.h"
#include "Data/Interaction/RetrieveInteractionTypeAsset.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "FileHelpers.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ObjectTools.h"
#endif

URetrieveMigrateInteractionPresetsCommandlet::URetrieveMigrateInteractionPresetsCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 URetrieveMigrateInteractionPresetsCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	struct FInteractionPresetMigration
	{
		FName PresetId;
		const TCHAR* PresetPath;
		const TCHAR* TypePath;
	};

	const FInteractionPresetMigration Migrations[] =
	{
		{ TEXT("PickupItem"), TEXT("/Game/Retrieve/Data/Interaction/DA_Preset_PickupItem.DA_Preset_PickupItem"), TEXT("/Game/Retrieve/Data/Interaction/DA_InteractionType_PickupItem.DA_InteractionType_PickupItem") },
		{ TEXT("OpenChest"), TEXT("/Game/Retrieve/Data/Interaction/DA_Preset_OpenChest.DA_Preset_OpenChest"), TEXT("/Game/Retrieve/Data/Interaction/DA_InteractionType_OpenChest.DA_InteractionType_OpenChest") },
		{ TEXT("MineOre"), TEXT("/Game/Retrieve/Data/Interaction/DA_Preset_MineOre.DA_Preset_MineOre"), TEXT("/Game/Retrieve/Data/Interaction/DA_InteractionType_MineOre.DA_InteractionType_MineOre") },
		{ TEXT("LootEnemy"), TEXT("/Game/Retrieve/Data/Interaction/DA_Preset_LootEnemy.DA_Preset_LootEnemy"), TEXT("/Game/Retrieve/Data/Interaction/DA_InteractionType_LootEnemy.DA_InteractionType_LootEnemy") },
		{ TEXT("Bonfire"), TEXT("/Game/Retrieve/Data/Interaction/DA_Preset_Bonfire.DA_Preset_Bonfire"), TEXT("/Game/Retrieve/Data/Interaction/DA_InteractionType_Bonfire.DA_InteractionType_Bonfire") },
	};

	constexpr const TCHAR* ProfilePackageName = TEXT("/Game/Retrieve/Data/Interaction/DA_InteractionPresets_Default");
	constexpr const TCHAR* ProfileAssetName = TEXT("DA_InteractionPresets_Default");
	UPackage* ProfilePackage = CreatePackage(ProfilePackageName);
	URetrieveInteractionPresetProfileAsset* Profile =
		LoadObject<URetrieveInteractionPresetProfileAsset>(nullptr, TEXT("/Game/Retrieve/Data/Interaction/DA_InteractionPresets_Default.DA_InteractionPresets_Default"));
	if (!Profile)
	{
		Profile = NewObject<URetrieveInteractionPresetProfileAsset>(
			ProfilePackage,
			ProfileAssetName,
			RF_Public | RF_Standalone);
		FAssetRegistryModule::AssetCreated(Profile);
	}

	TArray<UPackage*> PackagesToSave;
	TArray<UObject*> TypeAssetsToDelete;
	TArray<UObject*> PresetAssetsToDelete;
	TArray<FRetrieveInteractionPresetData> RebuiltPresets;
	int32 MigratedCount = 0;
	Profile->Modify();

	for (const FInteractionPresetMigration& Migration : Migrations)
	{
		URetrieveInteractionPresetAsset* Preset =
			LoadObject<URetrieveInteractionPresetAsset>(nullptr, Migration.PresetPath);
		URetrieveInteractionTypeAsset* TypeAsset =
			LoadObject<URetrieveInteractionTypeAsset>(nullptr, Migration.TypePath);

		if (!Preset)
		{
			UE_LOG(LogTemp, Warning, TEXT("[InteractionPresetMigration] Missing preset: %s"), Migration.PresetPath);
			continue;
		}

		if (TypeAsset)
		{
			Preset->Modify();
			Preset->DisplayText = TypeAsset->DisplayText;
			Preset->bHoldInteraction = TypeAsset->bHoldInteraction;
			Preset->HoldDuration = TypeAsset->HoldDuration;
			Preset->InteractionMontage = TypeAsset->InteractionMontage;
			Preset->MontagePlayRate = TypeAsset->MontagePlayRate;
			Preset->PromptIcon = TypeAsset->PromptIcon;
			Preset->PromptAccentColor = TypeAsset->PromptAccentColor;
			Preset->WidgetClassOverride = TypeAsset->WidgetClassOverride;
			Preset->MgrProp_Icon = TypeAsset->MgrProp_Icon;
			Preset->MgrProp_Color = TypeAsset->MgrProp_Color;
			Preset->MgrProp_WidgetClass = TypeAsset->MgrProp_WidgetClass;
			TypeAssetsToDelete.Add(TypeAsset);
		}

		Preset->TypeAsset = nullptr;
		FRetrieveInteractionPresetData& PresetData = RebuiltPresets.AddDefaulted_GetRef();
		PresetData.PresetId = Migration.PresetId;
		PresetData.DisplayText = Preset->DisplayText;
		PresetData.bHoldInteraction = Preset->bHoldInteraction;
		PresetData.HoldDuration = Preset->HoldDuration;
		PresetData.InteractionMontage = Preset->InteractionMontage;
		PresetData.MontagePlayRate = Preset->MontagePlayRate;
		PresetData.PromptIcon = Preset->PromptIcon;
		PresetData.PromptAccentColor = Preset->PromptAccentColor;
		PresetData.WidgetClassOverride = Preset->WidgetClassOverride;
		PresetData.MgrProp_Icon = Preset->MgrProp_Icon;
		PresetData.MgrProp_Color = Preset->MgrProp_Color;
		PresetData.MgrProp_WidgetClass = Preset->MgrProp_WidgetClass;
		PresetData.ResultAssets = Preset->ResultAssets;
		PresetAssetsToDelete.Add(Preset);
		++MigratedCount;
	}

	if (RebuiltPresets.Num() > 0)
	{
		Profile->Presets = RebuiltPresets;
		Profile->MarkPackageDirty();
		PackagesToSave.AddUnique(Profile->GetOutermost());
	}

	struct FBlueprintPresetBinding
	{
		const TCHAR* BlueprintPath;
		FName PresetId;
	};

	const FBlueprintPresetBinding BlueprintBindings[] =
	{
		{ TEXT("/Game/Retrieve/Blueprints/BP_BonFire.BP_BonFire"), TEXT("Bonfire") },
		{ TEXT("/Game/Retrieve/Blueprints/Interactables/BP_Interactable_TreasureChest.BP_Interactable_TreasureChest"), TEXT("OpenChest") },
		{ TEXT("/Game/Retrieve/Blueprints/Interactables/BP_Interactable_SingleItem.BP_Interactable_SingleItem"), TEXT("PickupItem") },
		{ TEXT("/Game/Retrieve/Blueprints/Interactables/BP_Interactable_OreVein.BP_Interactable_OreVein"), TEXT("MineOre") },
		{ TEXT("/Game/Retrieve/Blueprints/Interactables/BP_Interactable_LootEnemy.BP_Interactable_LootEnemy"), TEXT("LootEnemy") },
		{ TEXT("/Game/Retrieve/Blueprints/Interactables/BP_Prop_Chest_01.BP_Prop_Chest_01"), TEXT("OpenChest") },
	};

	int32 UpdatedBlueprints = 0;
	for (const FBlueprintPresetBinding& Binding : BlueprintBindings)
	{
		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, Binding.BlueprintPath);
		if (!Blueprint)
		{
			UE_LOG(LogTemp, Warning, TEXT("[InteractionPresetMigration] Missing blueprint: %s"), Binding.BlueprintPath);
			continue;
		}

		bool bChangedBlueprint = false;
		auto ApplyToComponent = [&](URetrieveInteractionResponseComponent* Component)
		{
			if (!Component)
			{
				return;
			}

			Component->Modify();
			Component->PresetProfile = Profile;
			Component->PresetId = Binding.PresetId;
			Component->Preset = nullptr;
			bChangedBlueprint = true;
		};

		if (Blueprint->GeneratedClass)
		{
			if (AActor* CDO = Cast<AActor>(Blueprint->GeneratedClass->GetDefaultObject()))
			{
				TArray<URetrieveInteractionResponseComponent*> Components;
				CDO->GetComponents(Components);
				for (URetrieveInteractionResponseComponent* Component : Components)
				{
					ApplyToComponent(Component);
				}
			}
		}

		if (Blueprint->SimpleConstructionScript)
		{
			for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
			{
				ApplyToComponent(Cast<URetrieveInteractionResponseComponent>(Node ? Node->ComponentTemplate : nullptr));
			}
		}

		if (bChangedBlueprint)
		{
			Blueprint->Modify();
			FKismetEditorUtilities::CompileBlueprint(Blueprint);
			Blueprint->MarkPackageDirty();
			PackagesToSave.AddUnique(Blueprint->GetOutermost());
			++UpdatedBlueprints;
		}
	}

	if (PackagesToSave.Num() > 0 && !UEditorLoadingAndSavingUtils::SavePackages(PackagesToSave, false))
	{
		UE_LOG(LogTemp, Error, TEXT("[InteractionPresetMigration] Failed to save migrated assets."));
		return 2;
	}

	int32 DeletedCount = 0;
	if (TypeAssetsToDelete.Num() > 0)
	{
		DeletedCount = ObjectTools::DeleteObjectsUnchecked(TypeAssetsToDelete);
	}

	int32 DeletedPresetCount = 0;
	if (PresetAssetsToDelete.Num() > 0)
	{
		DeletedPresetCount = ObjectTools::DeleteObjectsUnchecked(PresetAssetsToDelete);
	}

	UE_LOG(LogTemp, Display,
		TEXT("[InteractionPresetMigration] Profile=%s MigratedPresets=%d UpdatedBlueprints=%d DeletedTypeAssets=%d DeletedPresetAssets=%d"),
		*GetNameSafe(Profile),
		MigratedCount,
		UpdatedBlueprints,
		DeletedCount,
		DeletedPresetCount);
	return 0;
#else
	UE_LOG(LogTemp, Error, TEXT("[InteractionPresetMigration] This commandlet requires an editor build."));
	return 1;
#endif
}
