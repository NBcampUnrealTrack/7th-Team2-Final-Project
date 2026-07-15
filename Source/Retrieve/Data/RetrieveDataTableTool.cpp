#include "Data/RetrieveDataTableTool.h"

#include "AbilitySystem/RetrieveAbilitySet.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Engine/DataTable.h"
#include "GameplayEffect.h"
#include "Logging/RetrieveLogChannels.h"

int32 URetrieveDataTableTool::SetArmorSetTagByRowPrefix(UDataTable* Table, const FString& RowPrefix, FGameplayTag SetTag)
{
	if (!Table || RowPrefix.IsEmpty())
	{
		return 0;
	}

	const UScriptStruct* RowStruct = Table->GetRowStruct();
	if (!RowStruct || !RowStruct->IsChildOf(FRetrieveArmorDataRow::StaticStruct()))
	{
		UE_LOG(LogRetrieveCombat, Warning, TEXT("[DataTool] %s is not an armor table"), *GetNameSafe(Table));
		return 0;
	}

	int32 Modified = 0;
	for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
	{
		if (!Pair.Key.ToString().StartsWith(RowPrefix))
		{
			continue;
		}

		if (FRetrieveArmorDataRow* Row = reinterpret_cast<FRetrieveArmorDataRow*>(Pair.Value))
		{
			Row->ArmorSetTag = SetTag;
			++Modified;
		}
	}

	if (Modified > 0)
	{
		Table->MarkPackageDirty();
	}
	UE_LOG(LogRetrieveCombat, Log, TEXT("[DataTool] SetArmorSetTag '%s' on %d rows (prefix=%s)"),
		*SetTag.ToString(), Modified, *RowPrefix);
	return Modified;
}

int32 URetrieveDataTableTool::SetWeaponAbilitySetByRowPrefix(UDataTable* Table, const FString& RowPrefix, const FString& AbilitySetObjectPath)
{
	if (!Table || RowPrefix.IsEmpty())
	{
		return 0;
	}

	const UScriptStruct* RowStruct = Table->GetRowStruct();
	if (!RowStruct || !RowStruct->IsChildOf(FRetrieveWeaponDataRow::StaticStruct()))
	{
		UE_LOG(LogRetrieveCombat, Warning, TEXT("[DataTool] %s is not a weapon table"), *GetNameSafe(Table));
		return 0;
	}

	const FSoftObjectPath AbilitySetPath(AbilitySetObjectPath);

	int32 Modified = 0;
	for (const TPair<FName, uint8*>& Pair : Table->GetRowMap())
	{
		if (!Pair.Key.ToString().StartsWith(RowPrefix))
		{
			continue;
		}

		if (FRetrieveWeaponDataRow* Row = reinterpret_cast<FRetrieveWeaponDataRow*>(Pair.Value))
		{
			Row->WeaponAbilitySet = AbilitySetPath;
			++Modified;
		}
	}

	if (Modified > 0)
	{
		Table->MarkPackageDirty();
	}
	UE_LOG(LogRetrieveCombat, Log, TEXT("[DataTool] SetWeaponAbilitySet '%s' on %d rows (prefix=%s)"),
		*AbilitySetObjectPath, Modified, *RowPrefix);
	return Modified;
}

bool URetrieveDataTableTool::AddGrantedEffectToAbilitySet(URetrieveAbilitySet* AbilitySet, TSubclassOf<UGameplayEffect> EffectClass, float EffectLevel)
{
	if (!AbilitySet || !EffectClass)
	{
		return false;
	}

	for (const FRetrieveAbilitySet_GameplayEffect& Existing : AbilitySet->GrantedGameplayEffects)
	{
		if (Existing.GameplayEffect == EffectClass)
		{
			return false;
		}
	}

	FRetrieveAbilitySet_GameplayEffect& NewEntry = AbilitySet->GrantedGameplayEffects.AddDefaulted_GetRef();
	NewEntry.GameplayEffect = EffectClass;
	NewEntry.EffectLevel = EffectLevel;
	AbilitySet->MarkPackageDirty();

	UE_LOG(LogRetrieveCombat, Log, TEXT("[DataTool] Added %s to %s (level %.1f)"),
		*GetNameSafe(EffectClass), *GetNameSafe(AbilitySet), EffectLevel);
	return true;
}
