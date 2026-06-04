#include "AbilitySystem/Element/GA_SetElement_Fire.h"

#include "GameplayTags/RetrieveGameplayTags.h"

UGA_SetElement_Fire::UGA_SetElement_Fire()
{
	ElementTag = RetrieveGameplayTags::Element_Fire;

	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_SetElement_Fire);
	SetAssetTags(Tags);

	ActivationBlockedTags.AddTag(RetrieveGameplayTags::Ability_Player_SetElement_Fire);
	ActivationPolicy = ERetrieveAbilityActivationPolicy::OnSpawn;
}
