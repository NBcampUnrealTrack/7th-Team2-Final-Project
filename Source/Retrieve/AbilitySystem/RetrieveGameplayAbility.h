#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "RetrieveGameplayAbility.generated.h"


UENUM(BlueprintType)
enum class ERetrieveAbilityActivationPolicy : uint8
{
	OnInputTriggered,
	OnSpawn,
	WhileInputActive // reserved; not used in MVP
};

UCLASS(Abstract)
class RETRIEVE_API URetrieveGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	URetrieveGameplayAbility(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	ERetrieveAbilityActivationPolicy GetActivationPolicy() const { return ActivationPolicy; }

	virtual void OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& AbilitySpec) override;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Ability")
	ERetrieveAbilityActivationPolicy ActivationPolicy = ERetrieveAbilityActivationPolicy::OnInputTriggered;
	
private:
	void TryActivateAbilityOnSpawn(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& AbilitySpec) const;
};
