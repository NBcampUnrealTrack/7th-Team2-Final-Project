#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "RetrieveAttributeSet.generated.h"

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

class URetrieveAbilitySystemComponent;

UCLASS()
class RETRIEVE_API URetrieveAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UWorld* GetWorld() const override;
	URetrieveAbilitySystemComponent* GetRetrieveAbilitySystemComponent() const;
};