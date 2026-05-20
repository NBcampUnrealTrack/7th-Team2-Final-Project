#include "AbilitySystem/Attributes/RetrieveAttributeSet.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"

UWorld* URetrieveAttributeSet::GetWorld() const
{
	const UObject* Outer = GetOuter();
	check(Outer);
	return Outer->GetWorld();
}

URetrieveAbilitySystemComponent* URetrieveAttributeSet::GetRetrieveAbilitySystemComponent() const
{
	return Cast<URetrieveAbilitySystemComponent>(GetOwningAbilitySystemComponent());
}