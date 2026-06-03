#include "AbilitySystem/Player/GA_HeavyAttack_Water.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "GameplayTags/RetrieveGameplayTags.h"

UGA_HeavyAttack_Water::UGA_HeavyAttack_Water()
{
	ActivationRequiredTags.AddTag(RetrieveGameplayTags::Element_Water);

	FGameplayTagContainer Tags;
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_HeavyAttack);
	Tags.AddTag(RetrieveGameplayTags::Ability_Player_HeavyAttack_Water);
	SetAssetTags(Tags);
	
	ActivationOwnedTags.AddTag(RetrieveGameplayTags::Animation_Lock_Movement);
}

void UGA_HeavyAttack_Water::ExecuteHeavyEffect(const FGameplayTag& /*ConsumedElement*/)
{
	ExecuteOwnerCue(RetrieveGameplayTags::GameplayCue_HeavyAttack_Water);
	
	if (HasAuthority(&GetCurrentActivationInfoRef()))
	{
		UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
		if (IsValid(ASC) && IsValid(ShieldEffectClass))
		{
			FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
			Ctx.AddSourceObject(this);

			FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(ShieldEffectClass, GetAbilityLevel(), Ctx);
			if (Spec.IsValid() && Spec.Data.IsValid())
			{
				ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
			}
		}
	}
	
	PlayHeavyMontageThenEnd();
}
