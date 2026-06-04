#include "AbilitySystem/RetrieveAbilitySet.h"
#include "AbilitySystem/RetrieveGameplayAbility.h"
#include "RetrieveAbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "AttributeSet.h"

void FRetrieveAbilitySet_GrantedHandles::AddAbilitySpecHandle(const FGameplayAbilitySpecHandle& Handle)
{
	if (Handle.IsValid())
	{
		AbilitySpecHandles.Add(Handle);
	}
}

void FRetrieveAbilitySet_GrantedHandles::AddGameplayEffectHandle(const FActiveGameplayEffectHandle& Handle)
{
	if (Handle.IsValid())
	{
		GameplayEffectHandles.Add(Handle);
	}
}

void FRetrieveAbilitySet_GrantedHandles::AddAttributeSet(UAttributeSet* Set)
{
	GrantedAttributeSets.Add(Set);
}

void FRetrieveAbilitySet_GrantedHandles::TakeFromAbilitySystem(URetrieveAbilitySystemComponent* ASC)
{
	if (!IsValid(ASC) || !ASC->IsOwnerActorAuthoritative())
	{
		return;
	}

	TArray<FGameplayAbilitySpecHandle> Handles = MoveTemp(AbilitySpecHandles);
	TArray<FActiveGameplayEffectHandle> EffectHandles = MoveTemp(GameplayEffectHandles);
	TArray<TObjectPtr<UAttributeSet>> AttributeSets = MoveTemp(GrantedAttributeSets);

	for (const FGameplayAbilitySpecHandle& Handle : Handles)
	{
		if (Handle.IsValid())
		{
			ASC->ClearAbility(Handle);
		}
	}

	for (const FActiveGameplayEffectHandle& Handle : EffectHandles)
	{
		if (Handle.IsValid())
		{
			ASC->RemoveActiveGameplayEffect(Handle);
		}
	}

	for (UAttributeSet* Set : AttributeSets)
	{
		if (Set)
		{
			ASC->RemoveSpawnedAttribute(Set);
		}
	}
}

void URetrieveAbilitySet::GiveToAbilitySystem(URetrieveAbilitySystemComponent* ASC,
                                              FRetrieveAbilitySet_GrantedHandles* OutGrantedHandles,
                                              UObject* SourceObject) const
{
	check(ASC);
	if (!ASC->IsOwnerActorAuthoritative()) return;

	// Grant the attribute sets
	for (const FRetrieveAbilitySet_AttributeSet& SetToGrant : GrantedAttributes)
	{
		if (!IsValid(SetToGrant.AttributeSet))
		{
			continue;
		}

		UAttributeSet* NewSet = NewObject<UAttributeSet>(ASC->GetOwner(), SetToGrant.AttributeSet);
		ASC->AddSpawnedAttribute(NewSet);

		if (OutGrantedHandles)
		{
			OutGrantedHandles->AddAttributeSet(NewSet);
		}
	}
	
	// Grant the gameplay effects
	for (const FRetrieveAbilitySet_GameplayEffect& EffectToGrant : GrantedGameplayEffects)
	{
		if (!IsValid(EffectToGrant.GameplayEffect))
		{
			continue;
		}

		const UGameplayEffect* GameplayEffect = EffectToGrant.GameplayEffect->GetDefaultObject<UGameplayEffect>();
		const FActiveGameplayEffectHandle Handle = ASC->ApplyGameplayEffectToSelf(
			GameplayEffect, EffectToGrant.EffectLevel, ASC->MakeEffectContext());

		if (OutGrantedHandles)
		{
			OutGrantedHandles->AddGameplayEffectHandle(Handle);
		}
	}

	// Grant the gameplay abilities
	for (const FRetrieveAbilitySet_GameplayAbility& AbilityToGrant : GrantedGameplayAbilities)
	{
		if (!IsValid(AbilityToGrant.Ability))
		{
			continue;
		}

		URetrieveGameplayAbility* AbilityCDO = AbilityToGrant.Ability->GetDefaultObject<URetrieveGameplayAbility>();

		FGameplayAbilitySpec Spec(AbilityCDO, AbilityToGrant.AbilityLevel);
		Spec.SourceObject = SourceObject;
		Spec.GetDynamicSpecSourceTags().AddTag(AbilityToGrant.InputTag);

		const FGameplayAbilitySpecHandle Handle = ASC->GiveAbility(Spec);

		if (OutGrantedHandles)
		{
			OutGrantedHandles->AddAbilitySpecHandle(Handle);
		}
	}
}
