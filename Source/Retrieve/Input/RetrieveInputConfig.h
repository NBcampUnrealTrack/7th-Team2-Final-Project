#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "RetrieveInputConfig.generated.h"

class UInputAction;

USTRUCT(BlueprintType)
struct FRetrieveInputAction
{
	GENERATED_BODY()
	
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UInputAction> InputAction = nullptr;
	
	UPROPERTY(EditDefaultsOnly, meta = (Categories = "Input,Ability"))
	FGameplayTag InputTag;
};

UCLASS(BlueprintType, Const)
class RETRIEVE_API URetrieveInputConfig : public UDataAsset
{
	GENERATED_BODY()
	
public:
	const UInputAction* FindNativeInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound = true) const;
	const UInputAction* FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound = true) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (TitleProperty = InputTag))
	TArray<FRetrieveInputAction> NativeInputActions;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (TitleProperty = InputTag))
	TArray<FRetrieveInputAction> AbilityInputActions;
};
