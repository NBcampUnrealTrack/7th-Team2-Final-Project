#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "RetrievePlayerState.generated.h"

class URetrieveAbilitySystemComponent;

UCLASS()
class RETRIEVE_API ARetrievePlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ARetrievePlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	URetrieveAbilitySystemComponent* GetRetrieveAbilitySystemComponent() const { return AbilitySystemComponent; }

protected:
	UPROPERTY(VisibleAnywhere, Category = "Retrieve|PlayerState")
	TObjectPtr<URetrieveAbilitySystemComponent> AbilitySystemComponent;
};
