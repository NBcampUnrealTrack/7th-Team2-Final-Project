#include "Data/Interaction/RetrieveLootTableAsset.h"

namespace
{
	/** 가중치 풀에서 한 번 추첨 — 가중치 비례 인덱스 반환. 0개거나 합이 0이면 INDEX_NONE */
	int32 PickWeightedIndex(const TArray<FRetrieveLootEntry>& Entries, FRandomStream& Stream)
	{
		// bGuaranteed는 추첨 풀에서 제외 (별도 처리). 또한 Weight<=0 도 제외.
		float TotalWeight = 0.0f;
		for (const FRetrieveLootEntry& Entry : Entries)
		{
			if (!Entry.bGuaranteed && Entry.Weight > 0.0f)
			{
				TotalWeight += Entry.Weight;
			}
		}

		if (TotalWeight <= 0.0f)
		{
			return INDEX_NONE;
		}

		float Roll = Stream.FRandRange(0.0f, TotalWeight);
		for (int32 Index = 0; Index < Entries.Num(); ++Index)
		{
			const FRetrieveLootEntry& Entry = Entries[Index];
			if (Entry.bGuaranteed || Entry.Weight <= 0.0f)
			{
				continue;
			}
			Roll -= Entry.Weight;
			if (Roll <= 0.0f)
			{
				return Index;
			}
		}

		// 부동소수 오차 보정 — 마지막 유효 엔트리 반환
		for (int32 Index = Entries.Num() - 1; Index >= 0; --Index)
		{
			const FRetrieveLootEntry& Entry = Entries[Index];
			if (!Entry.bGuaranteed && Entry.Weight > 0.0f)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	FRetrievePickupEntry MakePickupFromLoot(const FRetrieveLootEntry& Loot, FRandomStream& Stream)
	{
		FRetrievePickupEntry Out;
		Out.ItemId = Loot.ItemId;
		Out.ItemCategoryTag = Loot.ItemCategoryTag;
		const int32 Lo = FMath::Max(Loot.MinQuantity, 1);
		const int32 Hi = FMath::Max(Loot.MaxQuantity, Lo);
		Out.Quantity = Stream.RandRange(Lo, Hi);
		return Out;
	}
}

TArray<FRetrievePickupEntry> URetrieveLootTableAsset::RollLoot(FRandomStream& Stream) const
{
	TArray<FRetrievePickupEntry> Result;

	// 1) 보장 드롭 (bGuaranteed) — 항상 결과에 포함
	for (const FRetrieveLootEntry& Entry : Entries)
	{
		if (Entry.bGuaranteed && !Entry.ItemId.IsNone() && Entry.ItemCategoryTag.IsValid())
		{
			Result.Add(MakePickupFromLoot(Entry, Stream));
		}
	}

	// 2) 모드별 굴림
	if (bRollIndependent)
	{
		// 각 엔트리 독립 굴림 (DropChance 사용)
		for (const FRetrieveLootEntry& Entry : Entries)
		{
			if (Entry.bGuaranteed || Entry.ItemId.IsNone() || !Entry.ItemCategoryTag.IsValid())
			{
				continue;
			}
			if (Stream.FRand() <= Entry.DropChance)
			{
				Result.Add(MakePickupFromLoot(Entry, Stream));
			}
		}
	}
	else
	{
		// 가중치 풀 — MinRolls~MaxRolls 횟수만큼 추첨
		const int32 Lo = FMath::Max(MinRolls, 0);
		const int32 Hi = FMath::Max(MaxRolls, Lo);
		const int32 RollCount = Stream.RandRange(Lo, Hi);

		for (int32 RollIdx = 0; RollIdx < RollCount; ++RollIdx)
		{
			const int32 PickedIdx = PickWeightedIndex(Entries, Stream);
			if (Entries.IsValidIndex(PickedIdx))
			{
				const FRetrieveLootEntry& Loot = Entries[PickedIdx];
				if (!Loot.ItemId.IsNone() && Loot.ItemCategoryTag.IsValid())
				{
					Result.Add(MakePickupFromLoot(Loot, Stream));
				}
			}
		}
	}

	return Result;
}
