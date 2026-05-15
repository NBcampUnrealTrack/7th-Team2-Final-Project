#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystem/RetrieveAbilitySet.h"
#include "GameFramework/Actor.h"
#include "DebugAbilitySetActor.generated.h"

UCLASS()
class RETRIEVE_API ADebugAbilitySetActor : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
public:
	ADebugAbilitySetActor();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Debug")
	void TriggerDebugAbility();
	
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Debug")
	void ApplyTestDamage();
	
protected:
	virtual void BeginPlay() override;
	
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<URetrieveAbilitySystemComponent> ASC;
	
	UPROPERTY(EditAnywhere)
	TObjectPtr<URetrieveAbilitySet> AbilitySet;
	
	UPROPERTY()
	FRetrieveAbilitySet_GrantedHandles GrantedHandles;
	
	UPROPERTY(EditAnywhere, Category = "Debug")
	TSubclassOf<UGameplayEffect> TestDamageEffect;
};
