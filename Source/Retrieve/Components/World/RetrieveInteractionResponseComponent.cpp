// Retrieve Interaction Response Component ??Preset 湲곕컲 + AnimMontage ?ъ깮
#include "Components/World/RetrieveInteractionResponseComponent.h"

#include "Data/Interaction/RetrieveInteractionPresetAsset.h"
#include "Data/Interaction/RetrieveInteractionPresetProfileAsset.h"
#include "Data/Interaction/RetrieveInteractionResultAsset.h"
#include "Data/Interaction/RetrieveInteractionTypeAsset.h"
#include "Data/Interaction/RetrieveLootTableAsset.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Components/Inventory/InventoryComponent.h"
#include "Components/World/RetrieveMapIconComponent.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Blueprint/UserWidget.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Internationalization/Text.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/ShapeComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/UnrealType.h"

namespace RetrieveInteractionPrompt
{
	struct FPromptData
	{
		FText Text;
		TObjectPtr<UTexture2D> Icon = nullptr;
		FLinearColor AccentColor = FLinearColor::White;
	};

	template <typename RowType>
	FText FindDisplayNameInTable(const TCHAR* TablePath, FName ItemId, const TCHAR* Context)
	{
		if (UDataTable* DataTable = LoadObject<UDataTable>(nullptr, TablePath))
		{
			if (const RowType* Row = DataTable->FindRow<RowType>(ItemId, Context, false))
			{
				return Row->DisplayName;
			}
		}

		return FText::GetEmpty();
	}

	FText LookupItemDisplayName(FName ItemId, FGameplayTag ItemCategoryTag)
	{
		if (ItemId.IsNone())
		{
			return FText::GetEmpty();
		}

		if (ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Weapon))
		{
			const FText DisplayName = FindDisplayNameInTable<FRetrieveWeaponDataRow>(
				TEXT("/Game/Retrieve/Data/Items/DT_WeaponData.DT_WeaponData"),
				ItemId,
				TEXT("RetrieveInteractionPrompt"));
			if (!DisplayName.IsEmpty())
			{
				return DisplayName;
			}
		}
		else if (ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Consumable))
		{
			const FText DisplayName = FindDisplayNameInTable<FRetrieveConsumableItemRow>(
				TEXT("/Game/Retrieve/Data/Items/DT_ConsumableItem.DT_ConsumableItem"),
				ItemId,
				TEXT("RetrieveInteractionPrompt"));
			if (!DisplayName.IsEmpty())
			{
				return DisplayName;
			}
		}
		else if (ItemCategoryTag.MatchesTag(RetrieveGameplayTags::Item_Material))
		{
			const FText DisplayName = FindDisplayNameInTable<FRetrieveMaterialItemRow>(
				TEXT("/Game/Retrieve/Data/Items/DT_MaterialItem.DT_MaterialItem"),
				ItemId,
				TEXT("RetrieveInteractionPrompt"));
			if (!DisplayName.IsEmpty())
			{
				return DisplayName;
			}
		}

		const FText WeaponName = FindDisplayNameInTable<FRetrieveWeaponDataRow>(
			TEXT("/Game/Retrieve/Data/Items/DT_WeaponData.DT_WeaponData"),
			ItemId,
			TEXT("RetrieveInteractionPrompt"));
		if (!WeaponName.IsEmpty())
		{
			return WeaponName;
		}

		const FText ConsumableName = FindDisplayNameInTable<FRetrieveConsumableItemRow>(
			TEXT("/Game/Retrieve/Data/Items/DT_ConsumableItem.DT_ConsumableItem"),
			ItemId,
			TEXT("RetrieveInteractionPrompt"));
		if (!ConsumableName.IsEmpty())
		{
			return ConsumableName;
		}

		const FText MaterialName = FindDisplayNameInTable<FRetrieveMaterialItemRow>(
			TEXT("/Game/Retrieve/Data/Items/DT_MaterialItem.DT_MaterialItem"),
			ItemId,
			TEXT("RetrieveInteractionPrompt"));
		if (!MaterialName.IsEmpty())
		{
			return MaterialName;
		}

		return FText::FromName(ItemId);
	}

	bool LookupItemIconData(FName ItemId, FRetrieveItemIconRow& OutIconData)
	{
		if (ItemId.IsNone())
		{
			return false;
		}

		UDataTable* IconTable = LoadObject<UDataTable>(
			nullptr,
			TEXT("/Game/Retrieve/Data/Items/DT_ItemIcon.DT_ItemIcon"));
		if (!IconTable)
		{
			return false;
		}

		if (const FRetrieveItemIconRow* Row = IconTable->FindRow<FRetrieveItemIconRow>(
			ItemId,
			TEXT("RetrieveInteractionPrompt"),
			false))
		{
			OutIconData = *Row;
			return true;
		}

		return false;
	}
}

URetrieveInteractionResponseComponent::URetrieveInteractionResponseComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// ?대씪?댁뼵????Server RPC ?쇱슦?낆씠 ?꾩슂?섎?濡?而댄룷?뚰듃 ?먯껜 蹂듭젣 ?쒖꽦??
	// owner ?≫꽣??bReplicates = true?ъ빞 ?쒕떎 (?≫꽣 BP??Class Defaults?먯꽌 ?ㅼ젙).
	SetIsReplicatedByDefault(true);
}

void URetrieveInteractionResponseComponent::BeginPlay()
{
	Super::BeginPlay();

#if UE_BUILD_SHIPPING
	DisableShippingInteractionDebug();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			this, &URetrieveInteractionResponseComponent::DisableShippingInteractionDebug);
	}
#endif

	if (bAutoBindInteractionManager)
	{
		TryAutoBindInteractionManager();
	}

	if (bReopenAfterRests)
	{
		if (UWorld* World = GetWorld())
		{
			RestListenerHandle = UGameplayMessageSubsystem::Get(World)
				.RegisterListener<FRetrievePlayerRestedPayload>(
					RetrieveGameplayTags::Channel_Player_Rested,
					[WeakThis = TWeakObjectPtr<URetrieveInteractionResponseComponent>(this)]
					(FGameplayTag, const FRetrievePlayerRestedPayload&)
					{
						if (URetrieveInteractionResponseComponent* Comp = WeakThis.Get())
						{
							Comp->HandlePlayerRested();
						}
					});
		}
	}
}

void URetrieveInteractionResponseComponent::DisableShippingInteractionDebug()
{
#if UE_BUILD_SHIPPING
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	TInlineComponentArray<UActorComponent*> Components(OwnerActor);
	for (UActorComponent* Component : Components)
	{
		if (!Component)
		{
			continue;
		}

		if (Component->GetClass()->GetName().Contains(TEXT("Manager_InteractionTarget")))
		{
			if (FBoolProperty* EnableDebugProperty = FindFProperty<FBoolProperty>(
				Component->GetClass(), TEXT("EnableDebug")))
			{
				EnableDebugProperty->SetPropertyValue_InContainer(Component, false);
			}
		}

		// Interaction Manager의 디버그 존은 런타임 ShapeComponent로 표시된다.
		// 충돌 기능은 유지하고 렌더링만 차단한다.
		if (UShapeComponent* ShapeComponent = Cast<UShapeComponent>(Component))
		{
			// 캐릭터 루트 Capsule은 건드리지 않는다. 루트에 전파(propagate=true)로 렌더링을
			// 끄면 자식인 CharacterMesh0의 bHiddenInGame/bVisible까지 동시에 꺼져 메시가
			// 통째로 사라진다(루멘 메시 소실 버그의 직접 원인). 루프가 모든 Shape를 개별
			// 방문하므로 전파 없이도 디버그 존은 정상적으로 숨겨진다.
			if (ShapeComponent == OwnerActor->GetRootComponent())
			{
				continue;
			}

			ShapeComponent->SetHiddenInGame(true, false);
			ShapeComponent->SetVisibility(false, false);
		}
	}
#endif
}

void URetrieveInteractionResponseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		UGameplayMessageSubsystem::Get(World).UnregisterListener(RestListenerHandle);
	}
	Super::EndPlay(EndPlayReason);
}

void URetrieveInteractionResponseComponent::BeginDepletedState()
{
	bIsDepleted = true;
	RestsRemaining = FMath::Max(1, RestsUntilReopen);

	if (AActor* OwnerActor = GetOwner())
	{
		if (URetrieveMapIconComponent* MapIcon = OwnerActor->FindComponentByClass<URetrieveMapIconComponent>())
		{
			MapIcon->bIsDepleted = true;
		}
	}
}

void URetrieveInteractionResponseComponent::HandlePlayerRested()
{
	if (!bIsDepleted)
	{
		return;
	}

	if (--RestsRemaining > 0)
	{
		return;
	}

	bIsDepleted = false;

	if (AActor* OwnerActor = GetOwner())
	{
		if (URetrieveMapIconComponent* MapIcon = OwnerActor->FindComponentByClass<URetrieveMapIconComponent>())
		{
			MapIcon->bIsDepleted = false;
		}
	}

	OnReopened.Broadcast();
}

void URetrieveInteractionResponseComponent::ForcePersistentInteractionManager(UActorComponent* ManagerComp) const
{
	if (!ManagerComp)
	{
		return;
	}

	UClass* ManagerClass = ManagerComp->GetClass();
	bool bFinishMethodConfigured = false;

	if (FByteProperty* ByteProp =
		FindFProperty<FByteProperty>(ManagerClass, FName(TEXT("FinishMethod"))))
	{
		ByteProp->SetPropertyValue_InContainer(ManagerComp, ReopenFinishMethodValue);
		bFinishMethodConfigured = true;
	}
	else if (FEnumProperty* EnumProp =
		FindFProperty<FEnumProperty>(ManagerClass, FName(TEXT("FinishMethod"))))
	{
		EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(
			EnumProp->ContainerPtrToValuePtr<void>(ManagerComp),
			static_cast<int64>(ReopenFinishMethodValue));
		bFinishMethodConfigured = true;
	}

	if (FFloatProperty* DurationProp =
		FindFProperty<FFloatProperty>(ManagerClass, FName(TEXT("ReactivationDuration"))))
	{
		DurationProp->SetPropertyValue_InContainer(ManagerComp, 0.0f);
	}

	if (AActor* OwnerActor = GetOwner())
	{
		UE_LOG(LogTemp, Log,
			TEXT("[Retrieve|Interaction] %s: bReopenAfterRests - Manager_InteractionTarget FinishMethod %s"),
			*OwnerActor->GetName(),
			bFinishMethodConfigured ? TEXT("forced to reactivating value") : TEXT("property not found"));
	}
}

// ??????????????????????????????????????????????????????????????????????????????
// ?좏슚 媛??ы띁
// ??????????????????????????????????????????????????????????????????????????????

URetrieveInteractionTypeAsset* URetrieveInteractionResponseComponent::GetEffectiveTypeAsset() const
{
	// TypeAssetOverride媛 ?덉쑝硫???긽 ?곗꽑
	if (TypeAssetOverride)
	{
		return TypeAssetOverride.Get();
	}
	// ?놁쑝硫?Preset?먯꽌 ?쎄린
	if (Preset)
	{
		return Preset->TypeAsset.Get();
	}
	return nullptr;
}

const FRetrieveInteractionPresetData* URetrieveInteractionResponseComponent::GetEffectivePresetData() const
{
	if (!PresetProfile || PresetId.IsNone())
	{
		return nullptr;
	}

	for (const FRetrieveInteractionPresetData& PresetData : PresetProfile->Presets)
	{
		if (PresetData.PresetId == PresetId)
		{
			return &PresetData;
		}
	}

	return nullptr;
}

TArray<URetrieveInteractionResultAsset*> URetrieveInteractionResponseComponent::GetEffectiveResultAssets() const
{
	// ResultAssetsOverride媛 鍮꾩뼱?덉? ?딆쑝硫??곗꽑
	if (ResultAssetsOverride.Num() > 0)
	{
		TArray<URetrieveInteractionResultAsset*> Out;
		Out.Reserve(ResultAssetsOverride.Num());
		for (const TObjectPtr<URetrieveInteractionResultAsset>& Ptr : ResultAssetsOverride)
		{
			if (Ptr)
			{
				Out.Add(Ptr.Get());
			}
		}
		return Out;
	}
	if (const FRetrieveInteractionPresetData* PresetData = GetEffectivePresetData())
	{
		TArray<URetrieveInteractionResultAsset*> Out;
		Out.Reserve(PresetData->ResultAssets.Num());
		for (const TObjectPtr<URetrieveInteractionResultAsset>& Ptr : PresetData->ResultAssets)
		{
			if (Ptr)
			{
				Out.Add(Ptr.Get());
			}
		}
		return Out;
	}

	// ?놁쑝硫?Preset?먯꽌 ?쎄린
	if (Preset)
	{
		TArray<URetrieveInteractionResultAsset*> Out;
		Out.Reserve(Preset->ResultAssets.Num());
		for (const TObjectPtr<URetrieveInteractionResultAsset>& Ptr : Preset->ResultAssets)
		{
			if (Ptr)
			{
				Out.Add(Ptr.Get());
			}
		}
		return Out;
	}
	return TArray<URetrieveInteractionResultAsset*>{};
}

UAnimMontage* URetrieveInteractionResponseComponent::GetEffectiveMontage() const
{
	// 1?쒖쐞: ?≫꽣 ?몄뒪?댁뒪蹂?override
	if (MontageOverride)
	{
		return MontageOverride.Get();
	}
	if (TypeAssetOverride && TypeAssetOverride->InteractionMontage)
	{
		return TypeAssetOverride->InteractionMontage.Get();
	}
	if (const FRetrieveInteractionPresetData* PresetData = GetEffectivePresetData())
	{
		return PresetData->InteractionMontage.Get();
	}
	if (Preset && Preset->InteractionMontage)
	{
		return Preset->InteractionMontage.Get();
	}
	if (Preset && Preset->TypeAsset && Preset->TypeAsset->InteractionMontage)
	{
		return Preset->TypeAsset->InteractionMontage.Get();
	}
	return nullptr;
}

UAnimMontage* URetrieveInteractionResponseComponent::GetEffectiveVisualMeshMontage() const
{
	if (VisualMeshMontageOverride)
	{
		return VisualMeshMontageOverride.Get();
	}
	if (const FRetrieveInteractionPresetData* PresetData = GetEffectivePresetData())
	{
		return PresetData->VisualMeshMontage.Get();
	}
	return nullptr;
}

float URetrieveInteractionResponseComponent::GetEffectiveInteractionAnimationDuration() const
{
	const float PlayRate = GetEffectiveMontagePlayRate();
	if (PlayRate <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	float MaxDuration = 0.0f;

	if (const UAnimMontage* Montage = GetEffectiveMontage())
	{
		MaxDuration = FMath::Max(MaxDuration, Montage->GetPlayLength() / PlayRate);
	}
	if (const UAnimMontage* VisualMontage = GetEffectiveVisualMeshMontage())
	{
		MaxDuration = FMath::Max(MaxDuration, VisualMontage->GetPlayLength() / PlayRate);
	}

	return MaxDuration;
}

FText URetrieveInteractionResponseComponent::GetEffectiveDisplayText() const
{
	if (TypeAssetOverride)
	{
		return TypeAssetOverride->DisplayText;
	}
	if (const FRetrieveInteractionPresetData* PresetData = GetEffectivePresetData())
	{
		return PresetData->DisplayText;
	}
	if (Preset)
	{
		if (!Preset->DisplayText.IsEmpty())
		{
			return Preset->DisplayText;
		}
		if (Preset->TypeAsset)
		{
			return Preset->TypeAsset->DisplayText;
		}
	}
	// 프리셋/오버라이드가 전혀 없으면 빈 텍스트를 반환한다. 과거엔 "Interact"를 반환해
	// BP에 직접 설정된 Manager InteractionText(예: 루멘 "대화하기")를 덮어쓰는 문제가 있었다.
	// 빈 값이면 ApplyTypeAssetToManagerInternal이 텍스트 갱신을 건너뛴다.
	return FText::GetEmpty();
}

bool URetrieveInteractionResponseComponent::GetEffectiveHoldInteraction() const
{
	if (bOverrideHoldSettings)
	{
		return bHoldInteractionOverride;
	}
	if (TypeAssetOverride)
	{
		return TypeAssetOverride->bHoldInteraction;
	}
	if (const FRetrieveInteractionPresetData* PresetData = GetEffectivePresetData())
	{
		return PresetData->bHoldInteraction;
	}
	if (Preset)
	{
		if (Preset->bHoldInteraction)
		{
			return true;
		}
		if (Preset->TypeAsset)
		{
			return Preset->TypeAsset->bHoldInteraction;
		}
	}
	return false;
}

float URetrieveInteractionResponseComponent::GetEffectiveHoldDuration() const
{
	if (bOverrideHoldSettings)
	{
		return FMath::Max(HoldDurationOverride, 0.05f);
	}
	if (TypeAssetOverride)
	{
		return FMath::Max(TypeAssetOverride->HoldDuration, 0.05f);
	}
	if (const FRetrieveInteractionPresetData* PresetData = GetEffectivePresetData())
	{
		return FMath::Max(PresetData->HoldDuration, 0.05f);
	}
	if (Preset)
	{
		if (Preset->bHoldInteraction)
		{
			return FMath::Max(Preset->HoldDuration, 0.05f);
		}
		if (Preset->TypeAsset)
		{
			return FMath::Max(Preset->TypeAsset->HoldDuration, 0.05f);
		}
	}
	return 1.0f;
}

float URetrieveInteractionResponseComponent::GetEffectiveMontagePlayRate() const
{
	if (TypeAssetOverride)
	{
		return TypeAssetOverride->MontagePlayRate;
	}
	if (const FRetrieveInteractionPresetData* PresetData = GetEffectivePresetData())
	{
		return PresetData->MontagePlayRate;
	}
	if (Preset)
	{
		if (!FMath::IsNearlyEqual(Preset->MontagePlayRate, 1.0f))
		{
			return Preset->MontagePlayRate;
		}
		if (Preset->TypeAsset)
		{
			return Preset->TypeAsset->MontagePlayRate;
		}
	}
	return 1.0f;
}

UTexture2D* URetrieveInteractionResponseComponent::GetEffectivePromptIcon() const
{
	if (TypeAssetOverride)
	{
		return TypeAssetOverride->PromptIcon.Get();
	}
	if (const FRetrieveInteractionPresetData* PresetData = GetEffectivePresetData())
	{
		return PresetData->PromptIcon.Get();
	}
	if (Preset)
	{
		if (Preset->PromptIcon)
		{
			return Preset->PromptIcon.Get();
		}
		if (Preset->TypeAsset)
		{
			return Preset->TypeAsset->PromptIcon.Get();
		}
	}
	return nullptr;
}

FLinearColor URetrieveInteractionResponseComponent::GetEffectivePromptAccentColor() const
{
	if (bOverridePromptAccentColor)
	{
		return PromptAccentColorOverride;
	}
	if (TypeAssetOverride)
	{
		return TypeAssetOverride->PromptAccentColor;
	}
	if (const FRetrieveInteractionPresetData* PresetData = GetEffectivePresetData())
	{
		return PresetData->PromptAccentColor;
	}
	if (Preset)
	{
		if (Preset->TypeAsset && Preset->PromptAccentColor == FLinearColor(0.78f, 0.63f, 0.13f, 1.0f))
		{
			return Preset->TypeAsset->PromptAccentColor;
		}
		return Preset->PromptAccentColor;
	}
	return FLinearColor(0.78f, 0.63f, 0.13f, 1.0f);
}

FName URetrieveInteractionResponseComponent::GetEffectiveMgrPropIcon() const
{
	if (TypeAssetOverride)
	{
		return TypeAssetOverride->MgrProp_Icon;
	}
	if (const FRetrieveInteractionPresetData* PresetData = GetEffectivePresetData())
	{
		return PresetData->MgrProp_Icon;
	}
	if (Preset)
	{
		return Preset->MgrProp_Icon.IsNone() && Preset->TypeAsset
			? Preset->TypeAsset->MgrProp_Icon
			: Preset->MgrProp_Icon;
	}
	return TEXT("InteractionIcon");
}

FName URetrieveInteractionResponseComponent::GetEffectiveMgrPropColor() const
{
	if (TypeAssetOverride)
	{
		return TypeAssetOverride->MgrProp_Color;
	}
	if (const FRetrieveInteractionPresetData* PresetData = GetEffectivePresetData())
	{
		return PresetData->MgrProp_Color;
	}
	if (Preset)
	{
		return Preset->MgrProp_Color.IsNone() && Preset->TypeAsset
			? Preset->TypeAsset->MgrProp_Color
			: Preset->MgrProp_Color;
	}
	return TEXT("InteractionColor");
}


// ??????????????????????????????????????????????????????????????????????????????
// TypeAsset ??Manager_InteractionTarget ?곸슜
// ??????????????????????????????????????????????????????????????????????????????

void URetrieveInteractionResponseComponent::ApplyTypeAssetToManager()
{
	if (!GetEffectivePresetData() && !Preset && !GetEffectiveTypeAsset() && PromptTextOverride.IsEmpty() && !PromptIconOverride)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	TArray<UActorComponent*> Comps;
	OwnerActor->GetComponents(Comps);
	for (UActorComponent* Comp : Comps)
	{
		if (Comp && Comp->GetFName() == InteractionManagerComponentName)
		{
			ApplyTypeAssetToManagerInternal(Comp);
			return;
		}
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[Retrieve|Interaction] ApplyTypeAssetToManager: %s?먯꽌 '%s' 而댄룷?뚰듃瑜?李얠? 紐삵븿"),
		*OwnerActor->GetName(), *InteractionManagerComponentName.ToString());
}

void URetrieveInteractionResponseComponent::ApplyTypeAssetToManagerInternal(UActorComponent* ManagerComp)
{
	if (!ManagerComp)
	{
		return;
	}

	const UClass* ManagerClass = ManagerComp->GetClass();
	const FString OwnerName = GetOwner() ? GetOwner()->GetName() : TEXT("Unknown");
	RetrieveInteractionPrompt::FPromptData PromptData;
	PromptData.Text = GetEffectiveDisplayText();
	PromptData.Icon = bHideIconForNonItemPrompt ? nullptr : GetEffectivePromptIcon();
	PromptData.AccentColor = GetEffectivePromptAccentColor();

	if (bUseQuickPickupItemPrompt && !QuickPickupItemId.IsNone())
	{
		const FText ItemName = RetrieveInteractionPrompt::LookupItemDisplayName(
			QuickPickupItemId,
			QuickPickupItemCategoryTag);

		FFormatNamedArguments Args;
		Args.Add(TEXT("ItemName"), ItemName);
		Args.Add(TEXT("ActionText"), GetEffectiveDisplayText());
		Args.Add(TEXT("Quantity"), FText::AsNumber(QuickPickupQuantity));
		PromptData.Text = FText::Format(QuickPickupPromptFormat, Args);

		FRetrieveItemIconRow IconData;
		if (RetrieveInteractionPrompt::LookupItemIconData(QuickPickupItemId, IconData))
		{
			if (UTexture2D* LoadedIcon = IconData.IconTexture.LoadSynchronous())
			{
				PromptData.Icon = LoadedIcon;
			}
			PromptData.AccentColor = IconData.AccentColor;
		}
		else
		{
			PromptData.Icon = GetEffectivePromptIcon();
		}
	}

	if (!PromptTextOverride.IsEmpty())
	{
		PromptData.Text = PromptTextOverride;
	}

	if (PromptIconOverride)
	{
		PromptData.Icon = PromptIconOverride.Get();
	}

	// ?? 1) HoldSeconds (float) ????????????????????????????????????????????
	if (FFloatProperty* HoldProp = FindFProperty<FFloatProperty>(ManagerClass, FName("HoldSeconds")))
	{
		const float Duration = GetEffectiveHoldInteraction()
			? GetEffectiveHoldDuration()
			: 0.25f;
		HoldProp->SetPropertyValue_InContainer(ManagerComp, Duration);
		UE_LOG(LogTemp, Verbose,
			TEXT("[Retrieve|Interaction] %s: HoldSeconds=%.2f ?곸슜"), *OwnerName, Duration);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Retrieve|Interaction] Manager??'HoldSeconds' float ?꾨줈?쇳떚 ?놁쓬 ???ㅽ궢"));
	}

	// ?? 2) InteractionType (byte/enum: 0=Tap, 1=Hold, 2=Repeat) ??????????
	const bool bHoldInteraction = GetEffectiveHoldInteraction();
	const uint8 TypeValue = bHoldInteraction ? 1 : 0;
	bool bTypeSet = false;

	if (FByteProperty* ByteProp = FindFProperty<FByteProperty>(ManagerClass, FName("InteractionType")))
	{
		ByteProp->SetPropertyValue_InContainer(ManagerComp, TypeValue);
		bTypeSet = true;
	}
	else if (FEnumProperty* EnumProp = FindFProperty<FEnumProperty>(ManagerClass, FName("InteractionType")))
	{
		EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(
			EnumProp->ContainerPtrToValuePtr<void>(ManagerComp), static_cast<int64>(TypeValue));
		bTypeSet = true;
	}

	if (!bTypeSet)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Retrieve|Interaction] Manager??'InteractionType' ?꾨줈?쇳떚 ?놁쓬 ???ㅽ궢"));
	}
	else
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("[Retrieve|Interaction] %s: InteractionType=%d(%s) ?곸슜"),
			*OwnerName, TypeValue, bHoldInteraction ? TEXT("Hold") : TEXT("Tap"));
	}

	// ?? 3) InteractionText 留듭쓽 泥?踰덉㎏ ??ぉ 媛깆떊 ???????????????????????
	if (PromptData.Text.IsEmpty())
	{
		// 유효 텍스트 없음(프리셋·오버라이드 미설정): BP에 디자인된 매니저 문구(예: 루멘 "대화하기")를 보존한다.
	}
	else if (FMapProperty* TextMapProp = FindFProperty<FMapProperty>(ManagerClass, FName("InteractionText")))
	{
		FTextProperty* ValueTextProp = CastField<FTextProperty>(TextMapProp->ValueProp);
		if (ValueTextProp)
		{
			void* MapPtr = TextMapProp->ContainerPtrToValuePtr<void>(ManagerComp);
			FScriptMapHelper MapHelper(TextMapProp, MapPtr);

			if (MapHelper.Num() > 0)
			{
				for (int32 Idx = 0; Idx < MapHelper.GetMaxIndex(); ++Idx)
				{
					if (MapHelper.IsValidIndex(Idx))
					{
						ValueTextProp->SetPropertyValue(
							MapHelper.GetValuePtr(Idx), PromptData.Text);
						UE_LOG(LogTemp, Verbose,
							TEXT("[Retrieve|Interaction] %s: InteractionText[0]=\"%s\" ?곸슜"),
							*OwnerName, *PromptData.Text.ToString());
						break;
					}
				}
			}
			else
			{
				FIntProperty* KeyIntProp = CastField<FIntProperty>(TextMapProp->KeyProp);
				if (KeyIntProp)
				{
					const int32 NewIdx = MapHelper.AddDefaultValue_Invalid_NeedsRehash();
					KeyIntProp->SetPropertyValue(MapHelper.GetKeyPtr(NewIdx), 0);
					ValueTextProp->SetPropertyValue(
						MapHelper.GetValuePtr(NewIdx), PromptData.Text);
					MapHelper.Rehash();
					UE_LOG(LogTemp, Verbose,
						TEXT("[Retrieve|Interaction] %s: InteractionText[0]=\"%s\" ????ぉ 異붽?"),
						*OwnerName, *PromptData.Text.ToString());
				}
				else
				{
					UE_LOG(LogTemp, Warning,
						TEXT("[Retrieve|Interaction] InteractionText 留듭씠 鍮꾩뼱 ?덇퀬 ????낆씠 int32媛 ?꾨떂 ???띿뒪???곸슜 遺덇?. "
						     "?먮뵒?곗뿉??Manager_InteractionTarget??InteractionText[0]???섎룞?쇰줈 ?ㅼ젙?섏꽭??"));
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Retrieve|Interaction] InteractionText 留듭쓽 媛???낆씠 FText媛 ?꾨떂 ???ㅽ궢"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Retrieve|Interaction] Manager??'InteractionText' TMap ?꾨줈?쇳떚 ?놁쓬 ???ㅽ궢"));
	}

	// ?? 4) ?꾩씠肄??띿뒪泥???????????????????????????????????????????????????
	// TypeAsset.PromptIcon ??Manager[MgrProp_Icon] (UTexture2D* ObjectProperty)
	const FName IconPropName = GetEffectiveMgrPropIcon();
	if (!IconPropName.IsNone())
	{
		if (FObjectProperty* IconProp =
			FindFProperty<FObjectProperty>(ManagerClass, IconPropName))
		{
			// ????덉쟾: UTexture2D ?뱀? 洹?遺紐?UTexture, UObject) ?щ’?대㈃ ?곸슜
			if (IconProp->PropertyClass &&
				UTexture2D::StaticClass()->IsChildOf(IconProp->PropertyClass))
			{
				IconProp->SetObjectPropertyValue_InContainer(ManagerComp, PromptData.Icon.Get());
				UE_LOG(LogTemp, Verbose,
					TEXT("[Retrieve|Interaction] %s: Icon='%s' ?곸슜"),
					*OwnerName, PromptData.Icon ? *PromptData.Icon->GetName() : TEXT("None"));
			}
			else
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[Retrieve|Interaction] '%s' ?꾨줈?쇳떚媛 UTexture2D ?명솚 ??낆씠 ?꾨떂"),
					*IconPropName.ToString());
			}
		}
		else
		{
			UE_LOG(LogTemp, Verbose,
				TEXT("[Retrieve|Interaction] Manager??'%s' ?꾩씠肄??꾨줈?쇳떚 ?놁쓬 ???ㅽ궢 (MgrProp_Icon ?대쫫???뺤씤?섏꽭??"),
				*IconPropName.ToString());
		}
	}

	// ?? 5) 媛뺤“???????????????????????????????????????????????????????????
	// TypeAsset.PromptAccentColor ??Manager[MgrProp_Color]
	// FLinearColor ? FColor ?????쒕룄?쒕떎 (?곸슜 ?먯뀑留덈떎 ??낆씠 ?ㅻ? ???덉쓬)
	const FName ColorPropName = GetEffectiveMgrPropColor();
	if (!ColorPropName.IsNone())
	{
		bool bColorApplied = false;

		if (FStructProperty* ColorProp =
			FindFProperty<FStructProperty>(ManagerClass, ColorPropName))
		{
			if (ColorProp->Struct == TBaseStructure<FLinearColor>::Get())
			{
				*ColorProp->ContainerPtrToValuePtr<FLinearColor>(ManagerComp) =
					PromptData.AccentColor;
				bColorApplied = true;
			}
			else if (ColorProp->Struct == TBaseStructure<FColor>::Get())
			{
				*ColorProp->ContainerPtrToValuePtr<FColor>(ManagerComp) =
					PromptData.AccentColor.ToFColor(true);
				bColorApplied = true;
			}
		}

		if (bColorApplied)
		{
			UE_LOG(LogTemp, Verbose,
				TEXT("[Retrieve|Interaction] %s: AccentColor=(%.2f,%.2f,%.2f) ?곸슜"),
				*OwnerName,
				PromptData.AccentColor.R,
				PromptData.AccentColor.G,
				PromptData.AccentColor.B);
		}
		else
		{
			UE_LOG(LogTemp, Verbose,
				TEXT("[Retrieve|Interaction] Manager??'%s' ?됱긽 ?꾨줈?쇳떚 ?놁쓬 ???ㅽ궢 (MgrProp_Color ?대쫫???뺤씤?섏꽭??"),
				*ColorPropName.ToString());
		}
	}

}

// ??????????????????????????????????????????????????????????????????????????????
// InteractionManager ?먮룞 諛붿씤??
// ??????????????????????????????????????????????????????????????????????????????

void URetrieveInteractionResponseComponent::TryAutoBindInteractionManager()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	UActorComponent* TargetComp = nullptr;
	TArray<UActorComponent*> Comps;
	OwnerActor->GetComponents(Comps);
	for (UActorComponent* Comp : Comps)
	{
		if (Comp && Comp->GetFName() == InteractionManagerComponentName)
		{
			TargetComp = Comp;
			break;
		}
	}

	if (!TargetComp)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Retrieve|Interaction] %s?먯꽌 而댄룷?뚰듃 '%s'瑜?李얠? 紐삵빐 ?먮룞 諛붿씤???앸왂. "
			     "BP Manager_InteractionTarget??而댄룷?뚰듃 ?대쫫???뺤씤?섍굅??InteractionManagerComponentName??議곗젙?섏꽭??"),
			*OwnerActor->GetName(), *InteractionManagerComponentName.ToString());
		return;
	}

	CachedInteractionManagerComp = TargetComp;

	if (bReopenAfterRests)
	{
		ForcePersistentInteractionManager(TargetComp);
	}

	// TypeAsset ?ㅼ젙??Manager???곸슜
	if (GetEffectivePresetData() || Preset || GetEffectiveTypeAsset() || !PromptTextOverride.IsEmpty() || PromptIconOverride)
	{
		ApplyTypeAssetToManagerInternal(TargetComp);
	}

	UClass* TargetClass = TargetComp->GetClass();

	// OnInteractionBegin 諛붿씤??(而ㅼ뒪? ?꾨＼?꾪듃 ?꾩젽 ?앹꽦)
	if (FMulticastDelegateProperty* BeginProp = FindFProperty<FMulticastDelegateProperty>(
		TargetClass, FName(TEXT("OnInteractionBegin"))))
	{
		FScriptDelegate BeginDel;
		BeginDel.BindUFunction(this, FName(TEXT("HandleInteractionManagerBegin")));
		BeginProp->AddDelegate(MoveTemp(BeginDel), TargetComp);
	}
	else
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("[Retrieve|Interaction] '%s' 而댄룷?뚰듃??OnInteractionBegin ?놁쓬 ??而ㅼ뒪? ?꾨＼?꾪듃 Begin 諛붿씤???ㅽ궢"),
			*TargetComp->GetName());
	}

	// OnInteractionUpdated 諛붿씤??(Hold 吏꾪뻾???낅뜲?댄듃)
	if (FMulticastDelegateProperty* UpdatedProp = FindFProperty<FMulticastDelegateProperty>(
		TargetClass, FName(TEXT("OnInteractionUpdated"))))
	{
		FScriptDelegate UpdatedDel;
		UpdatedDel.BindUFunction(this, FName(TEXT("HandleInteractionManagerUpdated")));
		UpdatedProp->AddDelegate(MoveTemp(UpdatedDel), TargetComp);
	}
	else
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("[Retrieve|Interaction] '%s' 而댄룷?뚰듃??OnInteractionUpdated ?놁쓬 ??而ㅼ뒪? ?꾨＼?꾪듃 Updated 諛붿씤???ㅽ궢"),
			*TargetComp->GetName());
	}

	// OnInteractionEnd Multicast Delegate 諛붿씤??(reflection)
	FMulticastDelegateProperty* DelegateProp = FindFProperty<FMulticastDelegateProperty>(
		TargetClass, FName(TEXT("OnInteractionEnd")));

	if (!DelegateProp)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Retrieve|Interaction] '%s' 而댄룷?뚰듃??OnInteractionEnd 硫?곗틦?ㅽ듃 ?붿뒪?⑥쿂媛 ?놁뼱 ?먮룞 諛붿씤???ㅽ뙣"),
			*TargetComp->GetName());
		return;
	}

	FScriptDelegate ScriptDel;
	ScriptDel.BindUFunction(this, FName(TEXT("HandleInteractionManagerEnd")));
	DelegateProp->AddDelegate(MoveTemp(ScriptDel), TargetComp);

	URetrieveInteractionTypeAsset* EffType = GetEffectiveTypeAsset();
	UE_LOG(LogTemp, Log,
		TEXT("[Retrieve|Interaction] %s: %s ?먮룞 諛붿씤???꾨즺 (Begin/Updated/End, TypeAsset=%s)"),
		*OwnerActor->GetName(), *TargetComp->GetName(),
		EffType ? *EffType->GetName() : TEXT("None"));
}

// ??????????????????????????????????????????????????????????????????????????????
// ?곹샇?묒슜 泥섎━ ?먮쫫
// ??????????????????????????????????????????????????????????????????????????????

void URetrieveInteractionResponseComponent::HandleInteractionManagerBegin(APawn* InteractorPawn)
{
	if (InteractorPawn && ShouldPlayMontageDuringInteraction())
	{
		// 상자 뚜껑/보상 로직이 시작되기 전에 캐릭터 상호작용 몽타주를 먼저 재생한다.
		TryPlayInteractionAnim(InteractorPawn);
	}
}

void URetrieveInteractionResponseComponent::HandleInteractionManagerUpdated(APawn* InteractorPawn, float Progress)
{




}

void URetrieveInteractionResponseComponent::HandleInteractionManagerEnd(uint8 Result, APawn* InteractorPawn)
{
	UE_LOG(LogTemp, Log,
		TEXT("[Retrieve|Interaction] OnInteractionEnd ?섏떊: Result=%d, Pawn=%s"),
		Result, *GetNameSafe(InteractorPawn));

	// 而ㅼ뒪? ?꾨＼?꾪듃 ?④린湲?(?깃났/痍⑥냼/?ㅽ뙣 臾닿?)


	if (InteractorPawn && ShouldPlayMontageDuringInteraction())
	{
		// 성공이면 이 호출 다음에 보상 적용과 BP 뚜껑 오픈 이벤트가 실행된다.
		// 취소/실패일 때도 Begin에서 시작한 몽타주가 남지 않도록 정지한다.
		Multicast_StopInteractionAnim(
			InteractorPawn,
			GetEffectiveMontage(),
			GetEffectiveVisualMeshMontage(),
			0.1f);
	}

	if (Result != SuccessResultValue)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[Retrieve|Interaction] Result=%d??SuccessResultValue=%d? ?щ씪 ?곹샇?묒슜 臾댁떆"),
			Result, SuccessResultValue);
		return;
	}

	HandleInteractionApplied(InteractorPawn);
}


void URetrieveInteractionResponseComponent::HandleInteractionApplied(AActor* InteractionInstigator)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !InteractionInstigator)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Retrieve|Interaction] HandleInteractionApplied 臾댁떆: OwnerActor ?먮뒗 Instigator媛 鍮꾩뼱 ?덉쓬"));
		return;
	}

	if (OwnerActor->HasAuthority())
	{
		ApplyResultAuthoritative(InteractionInstigator);
	}
	else
	{
		Server_ApplyResult(InteractionInstigator);
	}
}

void URetrieveInteractionResponseComponent::Server_ApplyResult_Implementation(AActor* InteractionInstigator)
{
	ApplyResultAuthoritative(InteractionInstigator);
}

void URetrieveInteractionResponseComponent::ApplyResultAuthoritative(AActor* InteractionInstigator)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority() || !InteractionInstigator)
	{
		return;
	}

	// 이미 열려서 재오픈 대기 중이면 결과를 다시 적용하지 않는다.
	if (bReopenAfterRests && bIsDepleted)
	{
		return;
	}

	int32 AppliedCount = 0;

	// ??????????????????????????????????????????????????????????????????????
	// 寃곌낵 ?곸슜 ?곗꽑?쒖쐞
	//
	//  1?쒖쐞: ResultAssetsOverride / Preset.ResultAssets (湲곗〈 DA 湲곕컲 諛⑹떇)
	//         ??蹂듭옟??怨좎젙 蹂댁긽쨌Composite쨌CustomEvent ???뱀닔 耳?댁뒪
	//  2?쒖쐞: DirectLootTable (?쒕∼ ?뚯씠釉?吏곸젒 李몄“)
	//         ???곸옄쨌紐ъ뒪?걔룰킅留????뺣쪧 ?쒕∼
	//  3?쒖쐞: QuickPickupItemId (?몃씪???⑥닚 ?쎌뾽)
	//         ??諛붾떏 ?꾩씠?? DA ?뚯씪 0媛?
	// ??????????????????????????????????????????????????????????????????????

	const TArray<URetrieveInteractionResultAsset*> EffectiveResults = GetEffectiveResultAssets();

	if (EffectiveResults.Num() > 0)
	{
		// ?? 1?쒖쐞: DA ResultAsset 泥댁씤 ??????????????????????????????????
		for (URetrieveInteractionResultAsset* Result : EffectiveResults)
		{
			if (!Result) { continue; }
			Result->ApplyResult(OwnerActor, InteractionInstigator, OwnerActor);
			++AppliedCount;
		}
	}
	else if (DirectLootTable)
	{
		// ?? 2?쒖쐞: LootTable 吏곸젒 援대┝ ??????????????????????????????????
		UInventoryComponent* Inventory =
			InteractionInstigator->FindComponentByClass<UInventoryComponent>();

		if (Inventory)
		{
			FRandomStream Stream;
			Stream.GenerateNewSeed();

			const TArray<FRetrievePickupEntry> Drops = DirectLootTable->RollLoot(Stream);
			for (const FRetrievePickupEntry& Drop : Drops)
			{
				if (Drop.ItemId.IsNone()) { continue; }
				if (Inventory->AddItem(Drop.ItemId, Drop.ItemCategoryTag, Drop.Quantity))
				{
					++AppliedCount;
				}
				else
				{
					UE_LOG(LogTemp, Warning,
						TEXT("[Retrieve|Interaction] %s: LootTable item AddItem ?ㅽ뙣 ItemId=%s Tag=%s Quantity=%d"),
						*OwnerActor->GetName(),
						*Drop.ItemId.ToString(),
						*Drop.ItemCategoryTag.ToString(),
						Drop.Quantity);
				}
			}

			UE_LOG(LogTemp, Log,
				TEXT("[Retrieve|Interaction] %s: LootTable '%s' ??%d醫??쒕∼"),
				*OwnerActor->GetName(), *DirectLootTable->GetName(), AppliedCount);
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Retrieve|Interaction] %s: DirectLootTable ?ㅼ젙?먯?留?Instigator??InventoryComponent ?놁쓬"),
				*OwnerActor->GetName());
		}
	}
	else if (!QuickPickupItemId.IsNone())
	{
		// ?? 3?쒖쐞: ?몃씪???⑥닚 ?쎌뾽 ?????????????????????????????????????
		UInventoryComponent* Inventory =
			InteractionInstigator->FindComponentByClass<UInventoryComponent>();

		if (Inventory)
		{
			const bool bAdded = Inventory->AddItem(
				QuickPickupItemId, QuickPickupItemCategoryTag, QuickPickupQuantity);

			if (bAdded)
			{
				++AppliedCount;
				UE_LOG(LogTemp, Log,
					TEXT("[Retrieve|Interaction] %s: QuickPickup '%s' x%d 異붽?"),
					*OwnerActor->GetName(), *QuickPickupItemId.ToString(), QuickPickupQuantity);
			}
			else
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[Retrieve|Interaction] %s: QuickPickup '%s' AddItem ?ㅽ뙣 (?몃깽?좊━ 媛??李쇨굅???섎せ??ItemId)"),
					*OwnerActor->GetName(), *QuickPickupItemId.ToString());
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Retrieve|Interaction] %s: QuickPickupItemId ?ㅼ젙?먯?留?Instigator??InventoryComponent ?놁쓬"),
				*OwnerActor->GetName());
		}
	}
	else
	{
		// 寃곌낵 ?놁쓬 ??OnApplied留?釉뚮줈?쒖틦?ㅽ듃 (臾맞룸젅踰꽷룸??????꾩씠???녿뒗 ?곹샇?묒슜)
		UE_LOG(LogTemp, Log,
			TEXT("[Retrieve|Interaction] %s: 寃곌낵 ?ㅼ젙 ?놁쓬 ??OnApplied ?몃━寃뚯씠?몃쭔 broadcast (臾맞룸젅踰꽷룸????꾩슜 ?곹샇?묒슜)"),
			*OwnerActor->GetName());
	}

	// 2) ?좊땲硫붿씠?? TypeAsset 紐쏀?二??ъ깮 + BP override ?대깽??
	// OpenChest는 Begin에서 이미 재생했고, End에서 뚜껑 로직 전에 정지했다.
	if (!ShouldPlayMontageDuringInteraction())
	{
		TryPlayInteractionAnim(InteractionInstigator);
	}

	// 3) ?붾쾭洹?硫붿떆吏
#if !UE_BUILD_SHIPPING
	if (bShowDebugMessageOnApply && GEngine)
	{
		const FString TypeName = GetEffectiveDisplayText().ToString();
		const FString Msg = FString::Printf(
			TEXT("[Interaction] %s ??%s | %s | 寃곌낵:%d"),
			*OwnerActor->GetName(),
			*InteractionInstigator->GetName(),
			*TypeName,
			AppliedCount);
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, Msg);
	}

	// 4) BP ?꾩쿂由??몃━寃뚯씠??
#endif

	OnApplied.Broadcast(InteractionInstigator);

	// 5) 1?뚯꽦 ?≫꽣 destroy (SetLifeSpan?쇰줈 1 tick 吏??
	if (bDestroyOwnerOnApplied)
	{
		OwnerActor->SetLifeSpan(0.1f);
	}
	else if (bReopenAfterRests)
	{
		BeginDepletedState();
	}
}

// ??????????????????????????????????????????????????????????????????????????????
// ?좊땲硫붿씠??
// ??????????????????????????????????????????????????????????????????????????????

void URetrieveInteractionResponseComponent::TryPlayInteractionAnim(AActor* InteractionInstigator)
{
	// ?? 1) ?쒕쾭痢?BP 而ㅼ뒪? 泥섎━ ?????????????????????????????????????????
	OnPlayInteractionAnim(InteractionInstigator);

	// ?? 2) ?ъ깮??紐쏀?二?寃곗젙 (MontageOverride ??TypeAsset.Montage) ??????
	UAnimMontage* MontageToPlay = GetEffectiveMontage();
	if (!MontageToPlay)
	{
		return;
	}

	// ?? 3) PlayRate 寃곗젙 (TypeAsset?먯꽌 ?쎌쓬, MontageOverride ??湲곕낯媛? ?
	const float PlayRate = GetEffectiveMontagePlayRate();

	// ?? 4) 紐⑤뱺 ?대씪?댁뼵?몄뿉 硫?곗틦?ㅽ듃濡??ъ깮 ???????????????????????????
	UAnimMontage* VisualMontage = GetEffectiveVisualMeshMontage();
	Multicast_PlayInteractionAnim(InteractionInstigator, MontageToPlay, PlayRate, VisualMontage);
}

void URetrieveInteractionResponseComponent::Multicast_PlayInteractionAnim_Implementation(
	AActor* Instigator, UAnimMontage* Montage, float PlayRate, UAnimMontage* VisualMontage)
{
	if (!Instigator)
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(Instigator);
	if (!Character)
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("[Retrieve|Interaction] %s: Instigator(%s) is not ACharacter - skipping anim"),
			*GetOwner()->GetName(), *Instigator->GetName());
		return;
	}

	// 스켈레톤이 일치하는 SkeletalMeshComponent를 찾아 몽타주를 재생
	auto PlayOnMatchingSkeleton = [&](UAnimMontage* MontageToPlay)
	{
		if (!MontageToPlay)
		{
			return;
		}

		USkeleton* TargetSkeleton = MontageToPlay->GetSkeleton();
		if (!TargetSkeleton)
		{
			return;
		}

		TArray<USkeletalMeshComponent*> Meshes;
		Character->GetComponents<USkeletalMeshComponent>(Meshes);

		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (!Mesh || !Mesh->GetSkeletalMeshAsset())
			{
				continue;
			}
			USkeleton* MeshSkeleton = Mesh->GetSkeletalMeshAsset()->GetSkeleton();
			// 정확히 같은 스켈레톤이 아니어도 UE5 Compatible Skeleton으로 등록되어 있으면 재생 허용.
			// (인벤토리 프리뷰의 Montage_Play는 엔진 기본 호환성 검사를 타므로 이미 이렇게 동작 중 - 여기도 동일하게 맞춘다)
			// ※ IsCompatibleForEditor()는 이름 그대로 에디터 전용(#if WITH_EDITOR)이라 Shipping 패키징에서
			//   컴파일되지 않는다. 런타임 안전한 GetCompatibleSkeletons()로 "MeshSkeleton이 TargetSkeleton의
			//   애니메이션을 재생할 수 있는지"를 직접 판정한다(CompatibleSkeletons는 단방향 목록).
			bool bSkeletonCompatible = (MeshSkeleton == TargetSkeleton);
			if (!bSkeletonCompatible && MeshSkeleton)
			{
				const FSoftObjectPath TargetSkeletonPath(TargetSkeleton);
				for (const TSoftObjectPtr<USkeleton>& CompatibleSkeleton : MeshSkeleton->GetCompatibleSkeletons())
				{
					if (CompatibleSkeleton.ToSoftObjectPath() == TargetSkeletonPath)
					{
						bSkeletonCompatible = true;
						break;
					}
				}
			}
			if (!bSkeletonCompatible)
			{
				continue;
			}
			UAnimInstance* AnimInst = Mesh->GetAnimInstance();
			if (!AnimInst)
			{
				continue;
			}
			AnimInst->Montage_Play(MontageToPlay, PlayRate);
			UE_LOG(LogTemp, Log,
				TEXT("[Retrieve|Interaction] %s: Montage '%s' on mesh '%s' (Rate=%.2f)"),
				*GetOwner()->GetName(), *MontageToPlay->GetName(), *Mesh->GetName(), PlayRate);
			return;
		}

		// 정확히 일치/호환 스켈레톤을 못 찾았어도 포기하지 않는다.
		// UAnimInstance::Montage_Play는 스켈레톤 일치 여부를 검사하지 않고(엔진 소스 AnimInstance.cpp의
		// Montage_PlayInternal은 둘 다 non-null인지만 확인), 본 매핑은 이름 기반으로 평가 시점에 처리된다.
		// 인벤토리 프리뷰의 방어구 장착 몽타주(AM_EquipChest 등, Synty 스켈레톤)도 이 방식 그대로
		// Character->GetMesh()의 AnimInstance에 직접 Montage_Play해서 정상 작동한다 - 여기서도 동일하게
		// 메인 메시에 바로 재생을 시도한다.
		if (USkeletalMeshComponent* MainMesh = Character->GetMesh())
		{
			if (UAnimInstance* AnimInst = MainMesh->GetAnimInstance())
			{
				AnimInst->Montage_Play(MontageToPlay, PlayRate);
				UE_LOG(LogTemp, Log,
					TEXT("[Retrieve|Interaction] %s: Montage '%s' on main mesh '%s' (Rate=%.2f, skeleton fallback)"),
					*GetOwner()->GetName(), *MontageToPlay->GetName(), *MainMesh->GetName(), PlayRate);
				return;
			}
		}

		UE_LOG(LogTemp, Warning,
			TEXT("[Retrieve|Interaction] %s: No AnimInstance available to play montage '%s'"),
			*GetOwner()->GetName(), *MontageToPlay->GetName());
	};

	PlayOnMatchingSkeleton(Montage);
	PlayOnMatchingSkeleton(VisualMontage);
}

void URetrieveInteractionResponseComponent::Multicast_StopInteractionAnim_Implementation(
	AActor* Instigator, UAnimMontage* Montage, UAnimMontage* VisualMontage, float BlendOutTime)
{
	ACharacter* Character = Cast<ACharacter>(Instigator);
	if (!Character)
	{
		return;
	}

	TSet<UAnimMontage*> MontagesToStop;
	if (Montage)
	{
		MontagesToStop.Add(Montage);
	}
	if (VisualMontage)
	{
		MontagesToStop.Add(VisualMontage);
	}

	TArray<USkeletalMeshComponent*> Meshes;
	Character->GetComponents<USkeletalMeshComponent>(Meshes);
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		UAnimInstance* AnimInstance = Mesh ? Mesh->GetAnimInstance() : nullptr;
		if (!AnimInstance)
		{
			continue;
		}

		for (UAnimMontage* MontageToStop : MontagesToStop)
		{
			if (AnimInstance->Montage_IsPlaying(MontageToStop))
			{
				AnimInstance->Montage_Stop(FMath::Max(0.0f, BlendOutTime), MontageToStop);
			}
		}
	}
}

bool URetrieveInteractionResponseComponent::ShouldPlayMontageDuringInteraction() const
{
	static const FName OpenChestPresetId(TEXT("OpenChest"));
	if (PresetId == OpenChestPresetId)
	{
		return true;
	}

	const auto IsOpenChestMontage = [](const UAnimMontage* Montage)
	{
		return Montage && Montage->GetName().StartsWith(TEXT("AM_OpenChest"));
	};

	return IsOpenChestMontage(GetEffectiveMontage())
		|| IsOpenChestMontage(GetEffectiveVisualMeshMontage());
}
