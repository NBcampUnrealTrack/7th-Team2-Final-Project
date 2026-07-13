#include "Character/Cosmetics/RetrieveModularMeshTypes.h"

#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInterface.h"

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

void FRetrieveArmorVisualPart::ApplyMaterialOverride(USkeletalMeshComponent* Component) const
{
	if (MaterialOverride.IsNull() || !IsValid(Component))
	{
		return;
	}

	UMaterialInterface* Material = MaterialOverride.LoadSynchronous();
	if (!Material)
	{
		return;
	}

	const int32 NumMaterials = Component->GetNumMaterials();
	for (int32 Index = 0; Index < NumMaterials; ++Index)
	{
		Component->SetMaterial(Index, Material);
	}
}
