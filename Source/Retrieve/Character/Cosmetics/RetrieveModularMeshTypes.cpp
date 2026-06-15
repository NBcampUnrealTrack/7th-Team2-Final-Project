#include "Character/Cosmetics/RetrieveModularMeshTypes.h"

URetrieveCharacterVisualLayout* FRetrieveVisualLayoutSelectionSet::SelectBestLayout(
	const FGameplayTagContainer& Tags) const
{
	for (const FRetrieveVisualLayoutSelectionEntry& Rule : LayoutRules)
	{
		if (Rule.Layout && Tags.HasAll(Rule.RequiredTags))
		{
			return Rule.Layout;
		}
	}

	return DefaultLayout;
}
