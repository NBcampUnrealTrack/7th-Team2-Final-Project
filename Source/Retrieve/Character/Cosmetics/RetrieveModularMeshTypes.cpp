
#include "Character/Cosmetics/RetrieveModularMeshTypes.h"

#include "Engine/SkeletalMesh.h"

TArray<FName> URetrieveModularMeshLayout::GetReferenceBoneNames() const
{
	TArray<FName> BoneNames;

	if (!ReferenceMesh)
	{
		return BoneNames;
	}

	const FReferenceSkeleton& ReferenceSkeleton = ReferenceMesh->GetRefSkeleton();
	const int32 BoneCount = ReferenceSkeleton.GetNum();
	BoneNames.Reserve(BoneCount);

	for (int32 BoneIndex = 0; BoneIndex < BoneCount; ++BoneIndex)
	{
		BoneNames.Add(ReferenceSkeleton.GetBoneName(BoneIndex));
	}

	return BoneNames;
}

TArray<FName> URetrieveModularPartSet::GetReferenceBoneNames() const
{
	if (!TargetLayout)
	{
		return TArray<FName>();
	}

	return TargetLayout->GetReferenceBoneNames();
}

URetrieveModularMeshLayout* FRetrieveModularLayoutSelectionSet::SelectBestLayout(const FGameplayTagContainer& Tags) const
{
	for (const FRetrieveModularLayoutSelectionEntry& Rule : LayoutRules)
	{
		if (Rule.Layout && Tags.HasAll(Rule.RequiredTags))		
		{
			return Rule.Layout;
		}
	}
	
	return DefaultLayout;
}
