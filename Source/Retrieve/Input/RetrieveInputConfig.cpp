#include "RetrieveInputConfig.h"

const UInputAction* URetrieveInputConfig::FindNativeInputActionForTag(const FGameplayTag& InputTag,
                                                                      bool bLogNotFound) const
{
	for (const FRetrieveInputAction& Action : NativeInputActions)
	{
		if (Action.InputAction && Action.InputTag == InputTag)
		{
			return Action.InputAction;
		}
	}
	if (bLogNotFound)
	{
		UE_LOG(LogTemp, Warning, TEXT("FindNativeInputActionForTag: %s not found in %s"), *InputTag.ToString(),
		       *GetNameSafe(this));
	}

	return nullptr;
}

const UInputAction* URetrieveInputConfig::FindAbilityInputActionForTag(const FGameplayTag& InputTag,
                                                                       bool bLogNotFound) const
{
	for (const FRetrieveInputAction& Action : AbilityInputActions)
	{
		if (Action.InputAction && Action.InputTag == InputTag) return Action.InputAction;
	}
	if (bLogNotFound)
	{
		UE_LOG(LogTemp, Warning, TEXT("FindAbilityInputActionForTag: %s not found in %s"), *InputTag.ToString(),
		       *GetNameSafe(this));
	}
	return nullptr;
}
