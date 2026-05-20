#include "Player/RetrievePlayerState.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"

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
