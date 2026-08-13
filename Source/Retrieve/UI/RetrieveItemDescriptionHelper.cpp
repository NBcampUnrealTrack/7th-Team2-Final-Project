#include "UI/RetrieveItemDescriptionHelper.h"

#include "Data/RetrieveDataTableTypes.h"
#include "Engine/DataTable.h"

#define LOCTEXT_NAMESPACE "RetrieveItemDescription"

namespace
{
	/** %.Nf 출력을 그대로 보존하기 위한 숫자 포맷 헬퍼. */
	FText DescNum(const float Value, const int32 FractionalDigits)
	{
		return FText::FromString(FString::Printf(TEXT("%.*f"), FractionalDigits, Value));
	}
}

FText URetrieveItemDescriptionHelper::BuildItemDescription(
	FName ItemId,
	FGameplayTag CategoryTag,
	UDataTable* ConsumableTable,
	UDataTable* MaterialTable,
	UDataTable* WeaponTable)
{
	if (ItemId.IsNone())
	{
		return FText::GetEmpty();
	}

	static const FGameplayTag WeaponParent     = FGameplayTag::RequestGameplayTag(TEXT("Item.Weapon"),     false);
	static const FGameplayTag ConsumableParent = FGameplayTag::RequestGameplayTag(TEXT("Item.Consumable"), false);
	static const FGameplayTag MaterialParent   = FGameplayTag::RequestGameplayTag(TEXT("Item.Material"),   false);

	if (CategoryTag.MatchesTag(WeaponParent) && WeaponTable)
	{
		if (const FRetrieveWeaponDataRow* Row =
			WeaponTable->FindRow<FRetrieveWeaponDataRow>(ItemId, TEXT("ItemDescriptionHelper::Weapon")))
		{
			return FormatWeapon(*Row);
		}
	}

	if (CategoryTag.MatchesTag(ConsumableParent) && ConsumableTable)
	{
		if (const FRetrieveConsumableItemRow* Row =
			ConsumableTable->FindRow<FRetrieveConsumableItemRow>(ItemId, TEXT("ItemDescriptionHelper::Consumable")))
		{
			return FormatConsumable(*Row);
		}
	}

	if (CategoryTag.MatchesTag(MaterialParent) && MaterialTable)
	{
		if (const FRetrieveMaterialItemRow* Row =
			MaterialTable->FindRow<FRetrieveMaterialItemRow>(ItemId, TEXT("ItemDescriptionHelper::Material")))
		{
			return FormatMaterial(*Row);
		}
	}

	return FText::GetEmpty();
}

FText URetrieveItemDescriptionHelper::FormatWeapon(const FRetrieveWeaponDataRow& Row)
{
	TArray<FString> Lines;

	Lines.Add(Row.DisplayName.IsEmpty() ? Row.ItemId.ToString() : Row.DisplayName.ToString());

	if (!Row.ShortDescription.IsEmpty())
	{
		Lines.Add(Row.ShortDescription.ToString());
	}

	TArray<FString> Stats;
	Stats.Add(FText::Format(LOCTEXT("Desc_Attack", "공격력: {0}"), DescNum(Row.AttackPower, 0)).ToString());
	if (Row.WeaponTypeTag.IsValid())
	{
		Stats.Add(FText::Format(LOCTEXT("Desc_Type", "타입: {0}"),
			FText::FromString(GetTagLeaf(Row.WeaponTypeTag))).ToString());
	}
	if (Row.WeaponGradeTag.IsValid())
	{
		Stats.Add(FText::Format(LOCTEXT("Desc_Grade", "등급: {0}"),
			FText::FromString(GetTagLeaf(Row.WeaponGradeTag))).ToString());
	}
	const FString Affinity = Row.WeaponAffinityTag.IsValid() ? GetTagLeaf(Row.WeaponAffinityTag) : FString();
	if (!Affinity.IsEmpty() && Affinity != TEXT("None"))
	{
		Stats.Add(FText::Format(LOCTEXT("Desc_Affinity", "친화도: {0}"),
			FText::FromString(Affinity)).ToString());
	}
	Lines.Add(FString::Join(Stats, TEXT("  |  ")));

	for (const FWeaponSkillPreview& Skill : Row.SkillPreviews)
	{
		FString SkillName = Skill.DisplayName.ToString();
		if (SkillName.IsEmpty() && Skill.AbilityTag.IsValid())
		{
			SkillName = GetTagLeaf(Skill.AbilityTag);
		}
		if (SkillName.IsEmpty())
		{
			continue;
		}

		if (Skill.ShortDescription.IsEmpty())
		{
			Lines.Add(FString::Printf(TEXT("[%s]"), *SkillName));
		}
		else
		{
			Lines.Add(FString::Printf(TEXT("[%s] %s"), *SkillName, *Skill.ShortDescription.ToString()));
		}
	}

	return FText::FromString(FString::Join(Lines, TEXT("\n")));
}

FText URetrieveItemDescriptionHelper::FormatConsumable(const FRetrieveConsumableItemRow& Row)
{
	TArray<FString> Lines;

	Lines.Add(Row.DisplayName.IsEmpty() ? Row.ItemId.ToString() : Row.DisplayName.ToString());

	if (!Row.ShortDescription.IsEmpty())
	{
		Lines.Add(Row.ShortDescription.ToString());
	}

	TArray<FString> Stats;
	if (Row.HealAmount > 0.0f)
	{
		Stats.Add(FText::Format(LOCTEXT("Desc_Heal", "회복량: {0}"), DescNum(Row.HealAmount, 0)).ToString());
	}
	if (Row.BuffDuration > 0.0f)
	{
		if (Row.ElementBuffMultiplier > 1.0f)
		{
			Stats.Add(FText::Format(LOCTEXT("Desc_ElementBuff", "원소 버프: x{0}"),
				DescNum(Row.ElementBuffMultiplier, 2)).ToString());
		}
		Stats.Add(FText::Format(LOCTEXT("Desc_Duration", "지속시간: {0}s"),
			DescNum(Row.BuffDuration, 1)).ToString());
	}
	if (!Stats.IsEmpty())
	{
		Lines.Add(FString::Join(Stats, TEXT("  |  ")));
	}

	return FText::FromString(FString::Join(Lines, TEXT("\n")));
}

FText URetrieveItemDescriptionHelper::FormatMaterial(const FRetrieveMaterialItemRow& Row)
{
	TArray<FString> Lines;

	Lines.Add(Row.DisplayName.IsEmpty() ? Row.ItemId.ToString() : Row.DisplayName.ToString());

	if (!Row.ShortDescription.IsEmpty())
	{
		Lines.Add(Row.ShortDescription.ToString());
	}

	return FText::FromString(FString::Join(Lines, TEXT("\n")));
}

FString URetrieveItemDescriptionHelper::GetTagLeaf(FGameplayTag Tag)
{
	const FString TagStr = Tag.GetTagName().ToString();
	int32 DotIndex = INDEX_NONE;
	return TagStr.FindLastChar(TEXT('.'), DotIndex) ? TagStr.RightChop(DotIndex + 1) : TagStr;
}

#undef LOCTEXT_NAMESPACE
