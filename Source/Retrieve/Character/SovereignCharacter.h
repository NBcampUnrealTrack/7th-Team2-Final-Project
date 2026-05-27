#pragma once

#include "CoreMinimal.h"
#include "Character/RetrieveCombatCharacter.h"
#include "GenericTeamAgentInterface.h"
#include "SovereignCharacter.generated.h"

class URetrievePawnCosmeticComponent;
class UCameraComponent;
class UInventoryComponent;
class URetrieveHeroComponent;
class USpringArmComponent;
class UCombatReactionComponent;
class UWeaponComponent;
class UElementGaugeComponent;

UCLASS()
class RETRIEVE_API ASovereignCharacter : public ARetrieveCombatCharacter, public IGenericTeamAgentInterface
{
	GENERATED_BODY()
	
public:
	ASovereignCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	virtual FGenericTeamId GetGenericTeamId() const override
	{
		return FGenericTeamId(PlayerTeamId);
	}
protected:
	virtual void InitializeAbilitySystem() override;
	virtual void UnPossessed() override;

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Components")
	TObjectPtr<URetrieveHeroComponent> HeroComponent;
	
	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Components")
	TObjectPtr<UCombatReactionComponent> CombatReactionComponent;

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Components")
	TObjectPtr<URetrievePawnCosmeticComponent> PawnCosmeticComponent;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<UInventoryComponent> InventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<UWeaponComponent> WeaponComponent;
	

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Components")
	TObjectPtr<UElementGaugeComponent> ElementGaugeComponent;

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Camera")
	TObjectPtr<USpringArmComponent> CameraSpringArm;

	UPROPERTY(VisibleAnywhere, Category = "Retrieve|Camera")
	TObjectPtr<UCameraComponent> ThirdPersonCamera;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|Team")
	uint8 PlayerTeamId = 1;
};
