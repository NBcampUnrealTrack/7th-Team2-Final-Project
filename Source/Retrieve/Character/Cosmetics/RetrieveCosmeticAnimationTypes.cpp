#include "Character/Cosmetics/RetrieveCosmeticAnimationTypes.h"

TSubclassOf<UAnimInstance> FRetrieveAnimLayerSelectionSet::SelectBestLayer(const FGameplayTagContainer& Tags) const
{
	for (const FRetrieveAnimLayerSelectionEntry& Rule : LayerRules)
	{
		if (Rule.Layer && Tags.HasAll(Rule.RequiredTags))
		{
			return Rule.Layer;
		}
	}
	
	return DefaultLayer;
}
