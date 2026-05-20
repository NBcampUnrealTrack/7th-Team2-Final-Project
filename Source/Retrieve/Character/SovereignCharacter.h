#pragma once

#include "CoreMinimal.h"
#include "Character/RetrieveCombatCharacter.h"
#include "SovereignCharacter.generated.h"

class UCameraComponent;
class URetrieveHeroComponent;
class USpringArmComponent;

UCLASS()
class RETRIEVE_API ASovereignCharacter : public ARetrieveCombatCharacter
{
	GENERATED_BODY()
	
public:
	ASovereignCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
protected:
	virtual void InitializeAbilitySystem() override;

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Components")
	TObjectPtr<URetrieveHeroComponent> HeroComponent;
	
	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Camera")
	TObjectPtr<USpringArmComponent> CameraSpringArm;

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Camera")
	TObjectPtr<UCameraComponent> ThirdPersonCamera;
};
