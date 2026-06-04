#include "AbilitySystem/Element/GA_SetElement_Water.h"

#include "GameplayTags/RetrieveGameplayTags.h"

UGA_SetElement_Water::UGA_SetElement_Water()
{
	ElementTag = RetrieveGameplayTags::Element_Water;

	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_SetElement_Water);
	SetAssetTags(Tags);

	ActivationBlockedTags.AddTag(RetrieveGameplayTags::Ability_Player_SetElement_Water);
}
