#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "Animation/AnimInstance.h"
#include "SovereignAnimInstance.generated.h"

class UAbilitySystemComponent;

UCLASS()
class RETRIEVE_API USovereignAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
	
public:
	USovereignAnimInstance(const FObjectInitializer& ObjectInitializer);
	virtual void InitializeWithAbilitySystem(UAbilitySystemComponent* ASC);
	
protected:
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
	
	virtual void NativeInitializeAnimation();
	virtual void NativeUpdateAnimation(float DeltaSeconds);
	
	UPROPERTY(EditDefaultsOnly, Category="GameplayTags")
	FGameplayTagBlueprintPropertyMap GameplayTagPropertyMap;
	
	UPROPERTY(BlueprintReadOnly, Category="Character State")
	float GroundDistance = -1.0f;
};
