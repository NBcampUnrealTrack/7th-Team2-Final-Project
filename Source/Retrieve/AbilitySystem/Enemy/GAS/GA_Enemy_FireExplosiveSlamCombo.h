#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Enemy/GAS/GA_EnemyPatternSequenceAttack.h"
#include "GA_Enemy_FireExplosiveSlamCombo.generated.h"

UCLASS(Blueprintable, BlueprintType)
class RETRIEVE_API UGA_Enemy_FireExplosiveSlamCombo : public UGA_EnemyPatternSequenceAttack
{
	GENERATED_BODY()

public:
	UGA_Enemy_FireExplosiveSlamCombo(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
