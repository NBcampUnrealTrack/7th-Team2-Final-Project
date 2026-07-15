#include "Components/Player/ArmorComponent.h"

#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Character/Cosmetics/RetrieveModularMeshTypes.h"
#include "Components/Pawn/RetrievePawnExtensionComponent.h"
#include "Components/Pawn/RetrievePawnCosmeticComponent.h"
#include "Engine/DataTable.h"
#include "GameplayEffect.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Net/UnrealNetwork.h"

UArmorComponent::UArmorComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UArmorComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UArmorComponent, EquippedArmorEntries);
}

void UArmorComponent::RefreshEquippedArmorGameplay()
{
    if (!HasAuthorityToModify())
    {
        RefreshArmorVisuals();
        return;
    }

    URetrieveAbilitySystemComponent* ASC = GetRetrieveAbilitySystemComponent();
    if (!ASC)
    {
        return;
    }

    // Startup equipment may be restored before the ASC is ready. Rebuild all
    // runtime armor effects from the current equipped entries once it is bound.
    ClearAllArmorGameplay();
    for (const TPair<FGameplayTag, FArmorSetBonusHandles>& Pair : ArmorSetBonusHandles)
    {
        if (Pair.Value.Bonus2.IsValid())
        {
            ASC->RemoveActiveGameplayEffect(Pair.Value.Bonus2);
        }
        if (Pair.Value.Bonus4.IsValid())
        {
            ASC->RemoveActiveGameplayEffect(Pair.Value.Bonus4);
        }
    }
    ArmorSetBonusHandles.Empty();

    for (const FRetrieveEquippedArmorEntry& Entry : EquippedArmorEntries)
    {
        if (!Entry.EquipmentSlotTag.IsValid() || Entry.ArmorItemId.IsNone())
        {
            continue;
        }

        if (const FRetrieveArmorDataRow* ArmorData = FindArmorData(Entry.ArmorItemId))
        {
            ApplyArmorGameplay(Entry.EquipmentSlotTag, *ArmorData);
        }
    }

    RefreshArmorVisuals();
    RecomputeSetBonuses();
}

bool UArmorComponent::EquipArmor(FGameplayTag EquipmentSlotTag, FName ArmorItemId)
{
	if (!HasAuthorityToModify())
	{
		return false;
	}

	if (!EquipmentSlotTag.IsValid() || ArmorItemId.IsNone())
	{
		return false;
	}

	const FRetrieveArmorDataRow* ArmorData = FindArmorData(ArmorItemId);
	if (!ArmorData || ArmorData->EquipmentSlotTag != EquipmentSlotTag)
	{
		return false;
	}

	if (FRetrieveEquippedArmorEntry* ExistingEntry = FindMutableEquippedArmorEntry(EquipmentSlotTag))
	{
		if (ExistingEntry->ArmorItemId == ArmorItemId)
		{
			return true;
		}

		ClearArmorGameplayForSlot(EquipmentSlotTag);
		ExistingEntry->ArmorItemId = ArmorItemId;
	}
	else
	{
		FRetrieveEquippedArmorEntry& NewEntry = EquippedArmorEntries.AddDefaulted_GetRef();
		NewEntry.EquipmentSlotTag = EquipmentSlotTag;
		NewEntry.ArmorItemId = ArmorItemId;
	}

	ApplyArmorGameplay(EquipmentSlotTag, *ArmorData);
	ApplyArmorVisual(EquipmentSlotTag, *ArmorData);
	RecomputeSetBonuses();
	OnArmorEquipped.Broadcast(EquipmentSlotTag, ArmorItemId);
	return true;
}

bool UArmorComponent::UnequipArmor(FGameplayTag EquipmentSlotTag)
{
	if (!HasAuthorityToModify() || !EquipmentSlotTag.IsValid())
	{
		return false;
	}

	for (int32 Index = 0; Index < EquippedArmorEntries.Num(); ++Index)
	{
		if (EquippedArmorEntries[Index].EquipmentSlotTag != EquipmentSlotTag)
		{
			continue;
		}

		const FName PreviousArmorId = EquippedArmorEntries[Index].ArmorItemId;
		EquippedArmorEntries.RemoveAt(Index);
		ClearArmorGameplayForSlot(EquipmentSlotTag);
		RecomputeSetBonuses();

		if (URetrievePawnCosmeticComponent* CosmeticComponent = GetPawnCosmeticComponent())
		{
			CosmeticComponent->ClearEquipmentVisualSlot(EquipmentSlotTag);
		}

		OnArmorUnequipped.Broadcast(EquipmentSlotTag, PreviousArmorId);
		return true;
	}

	return true;
}

void UArmorComponent::UnequipAllArmor()
{
	if (!HasAuthorityToModify())
	{
		return;
	}

	const TArray<FRetrieveEquippedArmorEntry> PreviousEntries = EquippedArmorEntries;
	EquippedArmorEntries.Reset();
	ClearAllArmorGameplay();
	RecomputeSetBonuses();

	if (URetrievePawnCosmeticComponent* CosmeticComponent = GetPawnCosmeticComponent())
	{
		CosmeticComponent->ClearAllEquipmentVisualSlots();
	}

	for (const FRetrieveEquippedArmorEntry& PreviousEntry : PreviousEntries)
	{
		OnArmorUnequipped.Broadcast(PreviousEntry.EquipmentSlotTag, PreviousEntry.ArmorItemId);
	}
}

FName UArmorComponent::GetEquippedArmorId(FGameplayTag EquipmentSlotTag) const
{
	const FRetrieveEquippedArmorEntry* Entry = FindEquippedArmorEntry(EquipmentSlotTag);
	return Entry ? Entry->ArmorItemId : NAME_None;
}

void UArmorComponent::OnRep_EquippedArmorEntries()
{
	RefreshArmorVisuals();
}

const FRetrieveArmorDataRow* UArmorComponent::FindArmorData(FName ArmorItemId) const
{
	if (!ArmorDataTable || ArmorItemId.IsNone())
	{
		return nullptr;
	}

	return ArmorDataTable->FindRow<FRetrieveArmorDataRow>(ArmorItemId, TEXT("UArmorComponent::FindArmorData"));
}

const FRetrieveEquippedArmorEntry* UArmorComponent::FindEquippedArmorEntry(FGameplayTag EquipmentSlotTag) const
{
	for (const FRetrieveEquippedArmorEntry& Entry : EquippedArmorEntries)
	{
		if (Entry.EquipmentSlotTag == EquipmentSlotTag)
		{
			return &Entry;
		}
	}
	return nullptr;
}

FRetrieveEquippedArmorEntry* UArmorComponent::FindMutableEquippedArmorEntry(FGameplayTag EquipmentSlotTag)
{
	return const_cast<FRetrieveEquippedArmorEntry*>(
		static_cast<const UArmorComponent*>(this)->FindEquippedArmorEntry(EquipmentSlotTag));
}

URetrieveAbilitySystemComponent* UArmorComponent::GetRetrieveAbilitySystemComponent() const
{
	const AActor* Owner = GetOwner();
	const URetrievePawnExtensionComponent* PawnExt = Owner
		? URetrievePawnExtensionComponent::FindPawnExtensionComponent(Owner)
		: nullptr;

	return PawnExt ? PawnExt->GetRetrieveAbilitySystemComponent() : nullptr;
}

URetrievePawnCosmeticComponent* UArmorComponent::GetPawnCosmeticComponent() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Owner->FindComponentByClass<URetrievePawnCosmeticComponent>() : nullptr;
}

bool UArmorComponent::HasAuthorityToModify() const
{
	const AActor* Owner = GetOwner();
	return !Owner || Owner->HasAuthority();
}

void UArmorComponent::ClearArmorGameplayForSlot(FGameplayTag EquipmentSlotTag)
{
	if (!HasAuthorityToModify() || !EquipmentSlotTag.IsValid())
	{
		return;
	}

	if (FActiveGameplayEffectHandle* DefenseHandle = ArmorDefenseEffectHandlesBySlot.Find(EquipmentSlotTag))
	{
		if (DefenseHandle->IsValid())
		{
			if (URetrieveAbilitySystemComponent* ASC = GetRetrieveAbilitySystemComponent())
			{
				ASC->RemoveActiveGameplayEffect(*DefenseHandle);
			}
		}
		ArmorDefenseEffectHandlesBySlot.Remove(EquipmentSlotTag);
	}

	if (FRetrieveAbilitySet_GrantedHandles* GrantedHandles = ArmorGrantedHandlesBySlot.Find(EquipmentSlotTag))
	{
		if (URetrieveAbilitySystemComponent* ASC = GetRetrieveAbilitySystemComponent())
		{
			GrantedHandles->TakeFromAbilitySystem(ASC);
		}
		ArmorGrantedHandlesBySlot.Remove(EquipmentSlotTag);
	}
}

void UArmorComponent::ClearAllArmorGameplay()
{
	if (!HasAuthorityToModify())
	{
		return;
	}

	TArray<FGameplayTag> EquipmentSlotTags;
	ArmorDefenseEffectHandlesBySlot.GenerateKeyArray(EquipmentSlotTags);
	for (const FGameplayTag& EquipmentSlotTag : EquipmentSlotTags)
	{
		ClearArmorGameplayForSlot(EquipmentSlotTag);
	}

	ArmorGrantedHandlesBySlot.GenerateKeyArray(EquipmentSlotTags);
	for (const FGameplayTag& EquipmentSlotTag : EquipmentSlotTags)
	{
		ClearArmorGameplayForSlot(EquipmentSlotTag);
	}
}

void UArmorComponent::ApplyArmorGameplay(FGameplayTag EquipmentSlotTag, const FRetrieveArmorDataRow& ArmorData)
{
	if (!HasAuthorityToModify() || !EquipmentSlotTag.IsValid())
	{
		return;
	}

	URetrieveAbilitySystemComponent* ASC = GetRetrieveAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	if (URetrieveAbilitySet* AbilitySet = Cast<URetrieveAbilitySet>(ArmorData.ArmorAbilitySet.TryLoad()))
	{
		FRetrieveAbilitySet_GrantedHandles& GrantedHandles = ArmorGrantedHandlesBySlot.FindOrAdd(EquipmentSlotTag);
		AbilitySet->GiveToAbilitySystem(ASC, &GrantedHandles, GetOwner());
	}

	if (ArmorDefenseEffect && ArmorData.Defense > 0.0f)
	{
		FGameplayEffectContextHandle EffectContext = ASC->MakeEffectContext();
		EffectContext.AddSourceObject(this);
		const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(
			ArmorDefenseEffect, 1.0f, EffectContext);
		if (SpecHandle.IsValid())
		{
			SpecHandle.Data->SetSetByCallerMagnitude(
				RetrieveGameplayTags::Data_Armor_Defense,
				ArmorData.Defense);
			const FActiveGameplayEffectHandle DefenseHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
			ArmorDefenseEffectHandlesBySlot.Add(EquipmentSlotTag, DefenseHandle);
		}
	}
}

const FRetrieveArmorSetBonusRow* UArmorComponent::FindSetBonusRow(FGameplayTag SetTag) const
{
	if (!ArmorSetBonusTable || !SetTag.IsValid())
	{
		return nullptr;
	}

	for (const TPair<FName, uint8*>& Pair : ArmorSetBonusTable->GetRowMap())
	{
		const FRetrieveArmorSetBonusRow* Row = reinterpret_cast<const FRetrieveArmorSetBonusRow*>(Pair.Value);
		if (Row && Row->SetTag == SetTag)
		{
			return Row;
		}
	}
	return nullptr;
}

void UArmorComponent::RecomputeSetBonuses()
{
	if (!HasAuthorityToModify())
	{
		return;
	}

	URetrieveAbilitySystemComponent* ASC = GetRetrieveAbilitySystemComponent();
	if (!ASC)
	{
		return;
	}

	// 컴포넌트에 테이블 미지정 시 기본 경로 폴백 (BP 재저장 없이 동작)
	if (!ArmorSetBonusTable)
	{
		ArmorSetBonusTable = Cast<UDataTable>(FSoftObjectPath(
			TEXT("/Game/Retrieve/Data/Items/DT_ArmorSetBonus.DT_ArmorSetBonus")).TryLoad());
		if (!ArmorSetBonusTable)
		{
			return;
		}
	}

	// 1. 착용 중 방어구의 세트별 부위 수 집계
	TMap<FGameplayTag, int32> PieceCounts;
	for (const FRetrieveEquippedArmorEntry& Entry : EquippedArmorEntries)
	{
		if (const FRetrieveArmorDataRow* ArmorData = FindArmorData(Entry.ArmorItemId))
		{
			if (ArmorData->ArmorSetTag.IsValid())
			{
				PieceCounts.FindOrAdd(ArmorData->ArmorSetTag)++;
			}
		}
	}

	// 2. 기존 핸들과 diff — 원하는 상태 계산 후 어긋난 것만 적용/회수
	TSet<FGameplayTag> SetTagsToVisit;
	for (const TPair<FGameplayTag, int32>& Pair : PieceCounts)
	{
		SetTagsToVisit.Add(Pair.Key);
	}
	for (const TPair<FGameplayTag, FArmorSetBonusHandles>& Pair : ArmorSetBonusHandles)
	{
		SetTagsToVisit.Add(Pair.Key);
	}

	auto ApplySetEffect = [this, ASC](const TSoftClassPtr<UGameplayEffect>& EffectPtr) -> FActiveGameplayEffectHandle
	{
		UClass* EffectClass = EffectPtr.LoadSynchronous();
		if (!EffectClass)
		{
			return FActiveGameplayEffectHandle();
		}
		FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
		Context.AddSourceObject(this);
		const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(EffectClass, 1.f, Context);
		return SpecHandle.IsValid() ? ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data) : FActiveGameplayEffectHandle();
	};

	for (const FGameplayTag& SetTag : SetTagsToVisit)
	{
		const int32 Count = PieceCounts.FindRef(SetTag);
		const FRetrieveArmorSetBonusRow* BonusRow = FindSetBonusRow(SetTag);
		const bool bWant2 = BonusRow && Count >= 2 && !BonusRow->Bonus2PieceEffect.IsNull();
		const bool bWant4 = BonusRow && Count >= 4 && !BonusRow->Bonus4PieceEffect.IsNull();

		FArmorSetBonusHandles& Handles = ArmorSetBonusHandles.FindOrAdd(SetTag);

		if (bWant2 && !Handles.Bonus2.IsValid())
		{
			Handles.Bonus2 = ApplySetEffect(BonusRow->Bonus2PieceEffect);
		}
		else if (!bWant2 && Handles.Bonus2.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(Handles.Bonus2);
			Handles.Bonus2 = FActiveGameplayEffectHandle();
		}

		if (bWant4 && !Handles.Bonus4.IsValid())
		{
			Handles.Bonus4 = ApplySetEffect(BonusRow->Bonus4PieceEffect);
		}
		else if (!bWant4 && Handles.Bonus4.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(Handles.Bonus4);
			Handles.Bonus4 = FActiveGameplayEffectHandle();
		}

		if (!Handles.Bonus2.IsValid() && !Handles.Bonus4.IsValid())
		{
			ArmorSetBonusHandles.Remove(SetTag);
		}
	}
}

bool UArmorComponent::ApplyArmorVisual(FGameplayTag EquipmentSlotTag, const FRetrieveArmorDataRow& ArmorData)
{
	URetrievePawnCosmeticComponent* CosmeticComponent = GetPawnCosmeticComponent();
	if (!CosmeticComponent)
	{
		return false;
	}

	// 방어구 외형은 row에 인라인된 파츠/억제 데이터로 로컬 재구성한다. (별도 DA 없음)
	CosmeticComponent->ApplyEquipmentPartsForSlot(
		EquipmentSlotTag,
		ArmorData.VisualParts,
		ArmorData.SuppressedDefaultPartSlots);
	return true;
}

void UArmorComponent::RefreshArmorVisuals()
{
	URetrievePawnCosmeticComponent* CosmeticComponent = GetPawnCosmeticComponent();
	if (!CosmeticComponent)
	{
		return;
	}

	CosmeticComponent->ClearAllEquipmentVisualSlots();
	for (const FRetrieveEquippedArmorEntry& Entry : EquippedArmorEntries)
	{
		if (!Entry.EquipmentSlotTag.IsValid() || Entry.ArmorItemId.IsNone())
		{
			continue;
		}

		if (const FRetrieveArmorDataRow* ArmorData = FindArmorData(Entry.ArmorItemId))
		{
			ApplyArmorVisual(Entry.EquipmentSlotTag, *ArmorData);
		}
	}
}
