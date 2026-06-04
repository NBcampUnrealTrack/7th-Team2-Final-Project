#include "AbilitySystem/Element/GA_SetElement_Wind.h"

#include "GameplayTags/RetrieveGameplayTags.h"

UGA_SetElement_Wind::UGA_SetElement_Wind()
{
	ElementTag = RetrieveGameplayTags::Element_Wind;

	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_SetElement_Wind);
	SetAssetTags(Tags);

	ActivationBlockedTags.AddTag(RetrieveGameplayTags::Ability_Player_SetElement_Wind);
}
