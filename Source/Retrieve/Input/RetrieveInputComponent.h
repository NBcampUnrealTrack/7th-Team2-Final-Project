#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "RetrieveInputConfig.h"
#include "RetrieveInputComponent.generated.h"

UCLASS(Config = Input)
class RETRIEVE_API URetrieveInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()

public:
	template <class UserClass, typename FuncType>
	void BindNativeAction(const URetrieveInputConfig* InputConfig, const FGameplayTag& InputTag,
	                      ETriggerEvent TriggerEvent, UserClass* Object, FuncType Func, bool bLogIfNotFound);

	template <class UserClass, typename PressedFuncType, typename ReleasedFuncType>
	void BindAbilityActions(const URetrieveInputConfig* InputConfig, UserClass* Object, PressedFuncType PressedFunc,
	                        ReleasedFuncType ReleasedFunc, TArray<uint32>& BindHandles);
};

template <class UserClass, typename FuncType>
void URetrieveInputComponent::BindNativeAction(const URetrieveInputConfig* InputConfig, const FGameplayTag& InputTag,
                                               ETriggerEvent TriggerEvent, UserClass* Object, FuncType Func,
                                               bool bLogIfNotFound)
{
	check(InputConfig);
	if (const UInputAction* IA = InputConfig->FindNativeInputActionForTag(InputTag, bLogIfNotFound))
	{
		BindAction(IA, TriggerEvent, Object, Func);
	}
}

template <class UserClass, typename PressedFuncType, typename ReleasedFuncType>
void URetrieveInputComponent::BindAbilityActions(const URetrieveInputConfig* InputConfig, UserClass* Object,
                                                 PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc,
                                                 TArray<uint32>& BindHandles)
{
	check(InputConfig);
	
	for (const FRetrieveInputAction& Action : InputConfig->AbilityInputActions)
	{
		if (Action.InputAction && Action.InputTag.IsValid())
		{
			if (PressedFunc)
			{
				BindHandles.Add(BindAction(Action.InputAction, ETriggerEvent::Started, Object, PressedFunc, Action.InputTag).GetHandle());
			}

			if (ReleasedFunc)
			{
				BindHandles.Add(BindAction(Action.InputAction, ETriggerEvent::Completed, Object, ReleasedFunc, Action.InputTag).GetHandle());
			}
		}
	}
}