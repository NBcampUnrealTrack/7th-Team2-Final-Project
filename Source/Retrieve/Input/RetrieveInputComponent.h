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
				// Triggered = 트리거 조건 완전 충족 시점.
				// Pressed 디폴트 트리거에선 Started==Triggered (즉시 발동).
				// Hold/Tap 같은 시간 기반 트리거에선 임계 조건이 정확히 작동.
				BindHandles.Add(BindAction(Action.InputAction, ETriggerEvent::Triggered, Object, PressedFunc, Action.InputTag).GetHandle());
			}

			if (ReleasedFunc)
			{
				BindHandles.Add(BindAction(Action.InputAction, ETriggerEvent::Completed, Object, ReleasedFunc, Action.InputTag).GetHandle());
			}
		}
	}
}