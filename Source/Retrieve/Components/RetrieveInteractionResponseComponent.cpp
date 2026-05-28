// Retrieve Interaction Response Component — Preset 기반 + AnimMontage 재생
#include "Components/RetrieveInteractionResponseComponent.h"

#include "Data/Interaction/RetrieveInteractionPresetAsset.h"
#include "Data/Interaction/RetrieveInteractionResultAsset.h"
#include "Data/Interaction/RetrieveInteractionTypeAsset.h"
#include "Data/Interaction/RetrieveLootTableAsset.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Components/InventoryComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Blueprint/UserWidget.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Internationalization/Text.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
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

	// 클라이언트 → Server RPC 라우팅이 필요하므로 컴포넌트 자체 복제 활성화.
	// owner 액터도 bReplicates = true여야 한다 (액터 BP의 Class Defaults에서 설정).
	SetIsReplicatedByDefault(true);
}

void URetrieveInteractionResponseComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoBindInteractionManager)
	{
		TryAutoBindInteractionManager();
	}
}

// ──────────────────────────────────────────────────────────────────────────────
// 유효 값 헬퍼
// ──────────────────────────────────────────────────────────────────────────────

URetrieveInteractionTypeAsset* URetrieveInteractionResponseComponent::GetEffectiveTypeAsset() const
{
	// TypeAssetOverride가 있으면 항상 우선
	if (TypeAssetOverride)
	{
		return TypeAssetOverride.Get();
	}
	// 없으면 Preset에서 읽기
	if (Preset)
	{
		return Preset->TypeAsset.Get();
	}
	return nullptr;
}

TArray<URetrieveInteractionResultAsset*> URetrieveInteractionResponseComponent::GetEffectiveResultAssets() const
{
	// ResultAssetsOverride가 비어있지 않으면 우선
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
	// 없으면 Preset에서 읽기
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

// ──────────────────────────────────────────────────────────────────────────────
// TypeAsset → Manager_InteractionTarget 적용
// ──────────────────────────────────────────────────────────────────────────────

void URetrieveInteractionResponseComponent::ApplyTypeAssetToManager()
{
	if (!GetEffectiveTypeAsset())
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
		TEXT("[Retrieve|Interaction] ApplyTypeAssetToManager: %s에서 '%s' 컴포넌트를 찾지 못함"),
		*OwnerActor->GetName(), *InteractionManagerComponentName.ToString());
}

void URetrieveInteractionResponseComponent::ApplyTypeAssetToManagerInternal(UActorComponent* ManagerComp)
{
	URetrieveInteractionTypeAsset* TypeAsset = GetEffectiveTypeAsset();
	if (!TypeAsset || !ManagerComp)
	{
		return;
	}

	const UClass* ManagerClass = ManagerComp->GetClass();
	const FString OwnerName = GetOwner() ? GetOwner()->GetName() : TEXT("Unknown");
	RetrieveInteractionPrompt::FPromptData PromptData;
	PromptData.Text = TypeAsset->DisplayText;
	PromptData.Icon = bHideIconForNonItemPrompt ? nullptr : TypeAsset->PromptIcon.Get();
	PromptData.AccentColor = TypeAsset->PromptAccentColor;

	if (bUseQuickPickupItemPrompt && !QuickPickupItemId.IsNone())
	{
		const FText ItemName = RetrieveInteractionPrompt::LookupItemDisplayName(
			QuickPickupItemId,
			QuickPickupItemCategoryTag);

		FFormatNamedArguments Args;
		Args.Add(TEXT("ItemName"), ItemName);
		Args.Add(TEXT("ActionText"), TypeAsset->DisplayText);
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
			PromptData.Icon = TypeAsset->PromptIcon.Get();
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

	// ── 1) HoldSeconds (float) ────────────────────────────────────────────
	if (FFloatProperty* HoldProp = FindFProperty<FFloatProperty>(ManagerClass, FName("HoldSeconds")))
	{
		const float Duration = TypeAsset->bHoldInteraction
			? TypeAsset->HoldDuration
			: 0.25f;  // Tap: 0.25s 빠른 게이지 피드백
		HoldProp->SetPropertyValue_InContainer(ManagerComp, Duration);
		UE_LOG(LogTemp, Verbose,
			TEXT("[Retrieve|Interaction] %s: HoldSeconds=%.2f 적용"), *OwnerName, Duration);
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Retrieve|Interaction] Manager에 'HoldSeconds' float 프로퍼티 없음 — 스킵"));
	}

	// ── 2) InteractionType (byte/enum: 0=Tap, 1=Hold, 2=Repeat) ──────────
	const uint8 TypeValue = TypeAsset->bHoldInteraction ? 1 : 0;
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
			TEXT("[Retrieve|Interaction] Manager에 'InteractionType' 프로퍼티 없음 — 스킵"));
	}
	else
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("[Retrieve|Interaction] %s: InteractionType=%d(%s) 적용"),
			*OwnerName, TypeValue, TypeAsset->bHoldInteraction ? TEXT("Hold") : TEXT("Tap"));
	}

	// ── 3) InteractionText 맵의 첫 번째 항목 갱신 ───────────────────────
	if (FMapProperty* TextMapProp = FindFProperty<FMapProperty>(ManagerClass, FName("InteractionText")))
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
							TEXT("[Retrieve|Interaction] %s: InteractionText[0]=\"%s\" 적용"),
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
						TEXT("[Retrieve|Interaction] %s: InteractionText[0]=\"%s\" 새 항목 추가"),
						*OwnerName, *PromptData.Text.ToString());
				}
				else
				{
					UE_LOG(LogTemp, Warning,
						TEXT("[Retrieve|Interaction] InteractionText 맵이 비어 있고 키 타입이 int32가 아님 — 텍스트 적용 불가. "
						     "에디터에서 Manager_InteractionTarget의 InteractionText[0]을 수동으로 설정하세요."));
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Retrieve|Interaction] InteractionText 맵의 값 타입이 FText가 아님 — 스킵"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Retrieve|Interaction] Manager에 'InteractionText' TMap 프로퍼티 없음 — 스킵"));
	}

	// ── 4) 아이콘 텍스처 ──────────────────────────────────────────────────
	// TypeAsset.PromptIcon → Manager[MgrProp_Icon] (UTexture2D* ObjectProperty)
	if (!TypeAsset->MgrProp_Icon.IsNone())
	{
		if (FObjectProperty* IconProp =
			FindFProperty<FObjectProperty>(ManagerClass, TypeAsset->MgrProp_Icon))
		{
			// 타입 안전: UTexture2D 혹은 그 부모(UTexture, UObject) 슬롯이면 적용
			if (IconProp->PropertyClass &&
				UTexture2D::StaticClass()->IsChildOf(IconProp->PropertyClass))
			{
				IconProp->SetObjectPropertyValue_InContainer(ManagerComp, PromptData.Icon.Get());
				UE_LOG(LogTemp, Verbose,
					TEXT("[Retrieve|Interaction] %s: Icon='%s' 적용"),
					*OwnerName, PromptData.Icon ? *PromptData.Icon->GetName() : TEXT("None"));
			}
			else
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[Retrieve|Interaction] '%s' 프로퍼티가 UTexture2D 호환 타입이 아님"),
					*TypeAsset->MgrProp_Icon.ToString());
			}
		}
		else
		{
			UE_LOG(LogTemp, Verbose,
				TEXT("[Retrieve|Interaction] Manager에 '%s' 아이콘 프로퍼티 없음 — 스킵 (MgrProp_Icon 이름을 확인하세요)"),
				*TypeAsset->MgrProp_Icon.ToString());
		}
	}

	// ── 5) 강조색 ─────────────────────────────────────────────────────────
	// TypeAsset.PromptAccentColor → Manager[MgrProp_Color]
	// FLinearColor 와 FColor 둘 다 시도한다 (상용 에셋마다 타입이 다를 수 있음)
	if (!TypeAsset->MgrProp_Color.IsNone())
	{
		bool bColorApplied = false;

		if (FStructProperty* ColorProp =
			FindFProperty<FStructProperty>(ManagerClass, TypeAsset->MgrProp_Color))
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
				TEXT("[Retrieve|Interaction] %s: AccentColor=(%.2f,%.2f,%.2f) 적용"),
				*OwnerName,
				PromptData.AccentColor.R,
				PromptData.AccentColor.G,
				PromptData.AccentColor.B);
		}
		else
		{
			UE_LOG(LogTemp, Verbose,
				TEXT("[Retrieve|Interaction] Manager에 '%s' 색상 프로퍼티 없음 — 스킵 (MgrProp_Color 이름을 확인하세요)"),
				*TypeAsset->MgrProp_Color.ToString());
		}
	}

	// ── 6) 위젯 클래스 override ───────────────────────────────────────────
	// TypeAsset.WidgetClassOverride → Manager[MgrProp_WidgetClass]
	// TSubclassOf<UUserWidget>(ClassProperty) 또는 TSoftClassPtr(SoftClassProperty) 시도
	if (!TypeAsset->WidgetClassOverride.IsNull() && !TypeAsset->MgrProp_WidgetClass.IsNone())
	{
		if (UClass* WidgetClass = TypeAsset->WidgetClassOverride.LoadSynchronous())
		{
			bool bClassApplied = false;

			if (FClassProperty* ClassProp =
				FindFProperty<FClassProperty>(ManagerClass, TypeAsset->MgrProp_WidgetClass))
			{
				ClassProp->SetPropertyValue_InContainer(ManagerComp, WidgetClass);
				bClassApplied = true;
			}
			else if (FSoftClassProperty* SoftProp =
				FindFProperty<FSoftClassProperty>(ManagerClass, TypeAsset->MgrProp_WidgetClass))
			{
				// FSoftClassProperty의 내부 값 타입은 FSoftObjectPtr.
				// UClass*(UObject* 하위)를 FSoftObjectPtr로 명시 변환.
				SoftProp->SetPropertyValue_InContainer(
					ManagerComp, FSoftObjectPtr(WidgetClass));
				bClassApplied = true;
			}

			if (bClassApplied)
			{
				UE_LOG(LogTemp, Verbose,
					TEXT("[Retrieve|Interaction] %s: WidgetClass='%s' 적용"),
					*OwnerName, *WidgetClass->GetName());
			}
			else
			{
				UE_LOG(LogTemp, Verbose,
					TEXT("[Retrieve|Interaction] Manager에 '%s' 위젯 클래스 프로퍼티 없음 — 스킵 (MgrProp_WidgetClass 이름을 확인하세요)"),
					*TypeAsset->MgrProp_WidgetClass.ToString());
			}
		}
	}
}

// ──────────────────────────────────────────────────────────────────────────────
// InteractionManager 자동 바인딩
// ──────────────────────────────────────────────────────────────────────────────

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
			TEXT("[Retrieve|Interaction] %s에서 컴포넌트 '%s'를 찾지 못해 자동 바인딩 생략. "
			     "BP Manager_InteractionTarget의 컴포넌트 이름을 확인하거나 InteractionManagerComponentName을 조정하세요."),
			*OwnerActor->GetName(), *InteractionManagerComponentName.ToString());
		return;
	}

	// TypeAsset 설정을 Manager에 적용
	if (GetEffectiveTypeAsset())
	{
		ApplyTypeAssetToManagerInternal(TargetComp);
	}

	// OnInteractionEnd Multicast Delegate 바인딩 (reflection)
	FMulticastDelegateProperty* DelegateProp = FindFProperty<FMulticastDelegateProperty>(
		TargetComp->GetClass(), FName(TEXT("OnInteractionEnd")));

	if (!DelegateProp)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Retrieve|Interaction] '%s' 컴포넌트에 OnInteractionEnd 멀티캐스트 디스패처가 없어 자동 바인딩 실패"),
			*TargetComp->GetName());
		return;
	}

	FScriptDelegate ScriptDel;
	ScriptDel.BindUFunction(this, FName(TEXT("HandleInteractionManagerEnd")));
	DelegateProp->AddDelegate(MoveTemp(ScriptDel), TargetComp);

	URetrieveInteractionTypeAsset* EffType = GetEffectiveTypeAsset();
	UE_LOG(LogTemp, Log,
		TEXT("[Retrieve|Interaction] %s: %s.OnInteractionEnd 자동 바인딩 완료 (TypeAsset=%s)"),
		*OwnerActor->GetName(), *TargetComp->GetName(),
		EffType ? *EffType->GetName() : TEXT("None"));
}

// ──────────────────────────────────────────────────────────────────────────────
// 상호작용 처리 흐름
// ──────────────────────────────────────────────────────────────────────────────

void URetrieveInteractionResponseComponent::HandleInteractionManagerEnd(uint8 Result, APawn* InteractorPawn)
{
	UE_LOG(LogTemp, Log,
		TEXT("[Retrieve|Interaction] OnInteractionEnd 수신: Result=%d, Pawn=%s"),
		Result, *GetNameSafe(InteractorPawn));

	if (Result != SuccessResultValue)
	{
		UE_LOG(LogTemp, Log,
			TEXT("[Retrieve|Interaction] Result=%d이 SuccessResultValue=%d와 달라 상호작용 무시"),
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
			TEXT("[Retrieve|Interaction] HandleInteractionApplied 무시: OwnerActor 또는 Instigator가 비어 있음"));
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

	int32 AppliedCount = 0;

	// ──────────────────────────────────────────────────────────────────────
	// 결과 적용 우선순위
	//
	//  1순위: ResultAssetsOverride / Preset.ResultAssets (기존 DA 기반 방식)
	//         → 복잡한 고정 보상·Composite·CustomEvent 등 특수 케이스
	//  2순위: DirectLootTable (드롭 테이블 직접 참조)
	//         → 상자·몬스터·광맥 등 확률 드롭
	//  3순위: QuickPickupItemId (인라인 단순 픽업)
	//         → 바닥 아이템, DA 파일 0개
	// ──────────────────────────────────────────────────────────────────────

	const TArray<URetrieveInteractionResultAsset*> EffectiveResults = GetEffectiveResultAssets();

	if (EffectiveResults.Num() > 0)
	{
		// ── 1순위: DA ResultAsset 체인 ──────────────────────────────────
		for (URetrieveInteractionResultAsset* Result : EffectiveResults)
		{
			if (!Result) { continue; }
			Result->ApplyResult(OwnerActor, InteractionInstigator, OwnerActor);
			++AppliedCount;
		}
	}
	else if (DirectLootTable)
	{
		// ── 2순위: LootTable 직접 굴림 ──────────────────────────────────
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
						TEXT("[Retrieve|Interaction] %s: LootTable item AddItem 실패 ItemId=%s Tag=%s Quantity=%d"),
						*OwnerActor->GetName(),
						*Drop.ItemId.ToString(),
						*Drop.ItemCategoryTag.ToString(),
						Drop.Quantity);
				}
			}

			UE_LOG(LogTemp, Log,
				TEXT("[Retrieve|Interaction] %s: LootTable '%s' → %d종 드롭"),
				*OwnerActor->GetName(), *DirectLootTable->GetName(), AppliedCount);
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Retrieve|Interaction] %s: DirectLootTable 설정됐지만 Instigator에 InventoryComponent 없음"),
				*OwnerActor->GetName());
		}
	}
	else if (!QuickPickupItemId.IsNone())
	{
		// ── 3순위: 인라인 단순 픽업 ─────────────────────────────────────
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
					TEXT("[Retrieve|Interaction] %s: QuickPickup '%s' x%d 추가"),
					*OwnerActor->GetName(), *QuickPickupItemId.ToString(), QuickPickupQuantity);
			}
			else
			{
				UE_LOG(LogTemp, Warning,
					TEXT("[Retrieve|Interaction] %s: QuickPickup '%s' AddItem 실패 (인벤토리 가득 찼거나 잘못된 ItemId)"),
					*OwnerActor->GetName(), *QuickPickupItemId.ToString());
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Retrieve|Interaction] %s: QuickPickupItemId 설정됐지만 Instigator에 InventoryComponent 없음"),
				*OwnerActor->GetName());
		}
	}
	else
	{
		// 결과 없음 — OnApplied만 브로드캐스트 (문·레버·대화 등 아이템 없는 상호작용)
		UE_LOG(LogTemp, Log,
			TEXT("[Retrieve|Interaction] %s: 결과 설정 없음 — OnApplied 델리게이트만 broadcast (문·레버·대화 전용 상호작용)"),
			*OwnerActor->GetName());
	}

	// 2) 애니메이션: TypeAsset 몽타주 재생 + BP override 이벤트
	TryPlayInteractionAnim(InteractionInstigator);

	// 3) 디버그 메시지
	if (bShowDebugMessageOnApply && GEngine)
	{
		URetrieveInteractionTypeAsset* TypeAsset = GetEffectiveTypeAsset();
		const FString TypeName = TypeAsset
			? TypeAsset->DisplayText.ToString()
			: TEXT("?");
		const FString Msg = FString::Printf(
			TEXT("[Interaction] %s ← %s | %s | 결과:%d"),
			*OwnerActor->GetName(),
			*InteractionInstigator->GetName(),
			*TypeName,
			AppliedCount);
		GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan, Msg);
	}

	// 4) BP 후처리 델리게이트
	OnApplied.Broadcast(InteractionInstigator);

	// 5) 1회성 액터 destroy (SetLifeSpan으로 1 tick 지연)
	if (bDestroyOwnerOnApplied)
	{
		OwnerActor->SetLifeSpan(0.1f);
	}
}

// ──────────────────────────────────────────────────────────────────────────────
// 애니메이션
// ──────────────────────────────────────────────────────────────────────────────

void URetrieveInteractionResponseComponent::TryPlayInteractionAnim(AActor* InteractionInstigator)
{
	// ── 1) BP 액터별 커스텀 처리 ────────────────────────────────────────
	OnPlayInteractionAnim(InteractionInstigator);

	// ── 2) TypeAsset의 AnimMontage를 instigator 캐릭터에 재생 ────────────
	URetrieveInteractionTypeAsset* TypeAsset = GetEffectiveTypeAsset();
	if (!TypeAsset || !TypeAsset->InteractionMontage)
	{
		return;
	}

	ACharacter* Character = Cast<ACharacter>(InteractionInstigator);
	if (!Character)
	{
		UE_LOG(LogTemp, Verbose,
			TEXT("[Retrieve|Interaction] %s: Instigator(%s)가 ACharacter가 아님 — 몽타주 스킵"),
			*GetOwner()->GetName(), *InteractionInstigator->GetName());
		return;
	}

	USkeletalMeshComponent* Mesh = Character->GetMesh();
	UAnimInstance* AnimInst = Mesh ? Mesh->GetAnimInstance() : nullptr;
	if (!AnimInst)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Retrieve|Interaction] %s: AnimInstance 획득 실패 — 몽타주 스킵"),
			*GetOwner()->GetName());
		return;
	}

	AnimInst->Montage_Play(TypeAsset->InteractionMontage);

	UE_LOG(LogTemp, Log,
		TEXT("[Retrieve|Interaction] %s: Montage '%s' 재생"),
		*GetOwner()->GetName(), *TypeAsset->InteractionMontage->GetName());
}
