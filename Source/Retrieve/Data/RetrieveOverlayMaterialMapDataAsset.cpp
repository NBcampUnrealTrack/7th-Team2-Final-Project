#include "Data/RetrieveOverlayMaterialMapDataAsset.h"

const FRetrieveOverlayEntry* URetrieveOverlayMaterialMapDataAsset::Find(const FGameplayTag& Tag) const
{
	return Map.Find(Tag);
}

void URetrieveOverlayMaterialMapDataAsset::GetKeyTags(FGameplayTagContainer& OutTags) const
{
	for (const TPair<FGameplayTag, FRetrieveOverlayEntry>& Pair : Map)
	{
		if (Pair.Key.IsValid())
		{
			OutTags.AddTag(Pair.Key);
		}
	}
}
