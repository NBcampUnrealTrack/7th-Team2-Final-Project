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

bool URetrieveElementUILibrary::GetMatchingBurstCombination(
	const UDataTable* SkillCombinationTable,
	const TMap<FGameplayTag, int32>& ElementPattern,
	FSkillCombination& OutCombination)
{
	if (!SkillCombinationTable || ElementPattern.IsEmpty())
	{
		return false;
	}

	for (auto It = SkillCombinationTable->GetRowMap().CreateConstIterator(); It; ++It)
	{
		const FSkillCombination* Row = reinterpret_cast<const FSkillCombination*>(It.Value());
		if (!Row || Row->ElementPattern.IsEmpty())
		{
			continue;
		}

		// 테이블 패턴의 모든 태그·수량이 현재 패턴과 일치하면 매칭
		bool bMatch = true;
		for (const auto& Pair : Row->ElementPattern)
		{
			const int32* Found = ElementPattern.Find(Pair.Key);
			if (!Found || *Found != Pair.Value)
			{
				bMatch = false;
				break;
			}
		}

		if (bMatch)
		{
			OutCombination = *Row;
			return true;
		}
	}

	return false;
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
	if (!Row)
	{
		return false;
	}

	OutRow = *Row;
	return true;
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
