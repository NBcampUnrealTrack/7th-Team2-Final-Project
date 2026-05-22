#include "Player/RetrievePlayerState.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"

ARetrievePlayerState::ARetrievePlayerState(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	AbilitySystemComponent = ObjectInitializer.CreateDefaultSubobject<URetrieveAbilitySystemComponent>(this, "AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
	
	SetNetUpdateFrequency(100.0f);
}

UAbilitySystemComponent* ARetrievePlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

FGameplayTag ARetrievePlayerState::GetCurrentElementTag() const
{
    if (!AbilitySystemComponent) return RetrieveGameplayTags::Element_None;

    if (AbilitySystemComponent->HasMatchingGameplayTag(RetrieveGameplayTags::Element_Fire))
        return RetrieveGameplayTags::Element_Fire;
    if (AbilitySystemComponent->HasMatchingGameplayTag(RetrieveGameplayTags::Element_Water))
        return RetrieveGameplayTags::Element_Water;
    if (AbilitySystemComponent->HasMatchingGameplayTag(RetrieveGameplayTags::Element_Wind))
        return RetrieveGameplayTags::Element_Wind;

    return RetrieveGameplayTags::Element_None;
}
