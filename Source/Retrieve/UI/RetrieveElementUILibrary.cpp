#include "UI/RetrieveElementUILibrary.h"

#include "Engine/DataTable.h"
#include "Components/Image.h"
#include "GameplayTags/RetrieveGameplayTags.h"

FLinearColor URetrieveElementUILibrary::ElementTagToColor(FGameplayTag ElementTag)
{
	if (ElementTag == RetrieveGameplayTags::Element_Fire)
	{
		return FLinearColor(1.0f, 0.45f, 0.0f, 1.0f);   // 주황 — 불
	}
	if (ElementTag == RetrieveGameplayTags::Element_Water)
	{
		return FLinearColor(0.1f, 0.6f, 1.0f, 1.0f);    // 파랑 — 물
	}
	if (ElementTag == RetrieveGameplayTags::Element_Wind)
	{
		return FLinearColor(0.35f, 1.0f, 0.35f, 1.0f);  // 초록 — 바람
	}

	// Element_None 또는 미정의 태그 → 회색 (빈 슬롯)
	return FLinearColor(0.3f, 0.3f, 0.3f, 1.0f);
}

FGameplayTag URetrieveElementUILibrary::ElementToAbsorbBuffUITag(FGameplayTag ElementTag)
{
	if (ElementTag.MatchesTagExact(RetrieveGameplayTags::Element_Fire))
	{
		return RetrieveGameplayTags::UI_Buff_Absorb_Fire;
	}
	if (ElementTag.MatchesTagExact(RetrieveGameplayTags::Element_Water))
	{
		return RetrieveGameplayTags::UI_Buff_Absorb_Water;
	}
	if (ElementTag.MatchesTagExact(RetrieveGameplayTags::Element_Wind))
	{
		return RetrieveGameplayTags::UI_Buff_Absorb_Wind;
	}
	return FGameplayTag();
}

bool URetrieveElementUILibrary::GetBurstCombinationByElement(
	const UDataTable* SkillCombinationTable,
	FGameplayTag WeaponType,
	FGameplayTag Element,
	FSkillCombination& OutCombination)
{
	if (!SkillCombinationTable)
	{
		return false;
	}

	auto FindRow = [SkillCombinationTable](FGameplayTag InWeapon, FGameplayTag InElement) -> const FSkillCombination*
	{
		for (auto It = SkillCombinationTable->GetRowMap().CreateConstIterator(); It; ++It)
		{
			const FSkillCombination* Row = reinterpret_cast<const FSkillCombination*>(It.Value());
			if (Row && Row->WeaponType == InWeapon && Row->BurstElement == InElement)
			{
				return Row;
			}
		}
		return nullptr;
	};

	const FSkillCombination* Found = FindRow(WeaponType, Element);
	if (!Found && Element != RetrieveGameplayTags::Element_None)
	{
		Found = FindRow(WeaponType, RetrieveGameplayTags::Element_None);
	}
	if (!Found)
	{
		return false;
	}

	OutCombination = *Found;
	return true;
}

bool URetrieveElementUILibrary::GetBuffUIRow(
	const UDataTable* BuffUITable,
	FGameplayTag BuffUITag,
	FRetrieveBuffUIRow& OutRow)
{
	if (!BuffUITable || !BuffUITag.IsValid())
	{
		return false;
	}

	const FName RowName = BuffUITag.GetTagName();
	const FRetrieveBuffUIRow* Row = BuffUITable->FindRow<FRetrieveBuffUIRow>(RowName, TEXT("GetBuffUIRow"));
	if (Row)
	{
		OutRow = *Row;
		return true;
	}

	for (auto It = BuffUITable->GetRowMap().CreateConstIterator(); It; ++It)
	{
		const FRetrieveBuffUIRow* Candidate = reinterpret_cast<const FRetrieveBuffUIRow*>(It.Value());
		if (Candidate && Candidate->BuffUITag.MatchesTagExact(BuffUITag))
		{
			OutRow = *Candidate;
			return true;
		}
	}

	return false;
}

void URetrieveElementUILibrary::SetImageBrushTexture(UImage* Image, UTexture2D* Texture, bool bMatchSize)
{
	if (Image)
	{
		Image->SetBrushFromTexture(Texture, bMatchSize);
	}
}

void URetrieveElementUILibrary::SetImageColor(UImage* Image, FLinearColor Color)
{
	if (Image)
	{
		Image->SetColorAndOpacity(Color);
	}
}
