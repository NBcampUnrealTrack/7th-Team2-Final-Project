#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Player/GA_HeavyAttack_Base.h"
#include "GA_HeavyAttack_Wind.generated.h"

/**
 * Wind 강공격 — 돌진/가속
 */
UCLASS()
class RETRIEVE_API UGA_HeavyAttack_Wind : public UGA_HeavyAttack_Base
{
	GENERATED_BODY()

public:
	UGA_HeavyAttack_Wind();

protected:
	virtual void ExecuteHeavyEffect(const FGameplayTag& ConsumedElement) override;

private:
	FVector ResolveLaunchDirection() const;
	
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|HeavyAttack|Wind", meta = (ClampMin = "0.0"))
	float LaunchSpeed = 1500.f;
};
