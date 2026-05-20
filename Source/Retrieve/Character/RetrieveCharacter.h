#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "RetrieveCharacter.generated.h"

class URetrieveAbilitySystemComponent;
class URetrievePawnData;
class URetrievePawnExtensionComponent;

/**
 * Pawn-level 베이스 클래스. 컨트롤러-PlayerState 핸드셰이크만 보유합니다. 
 * GetAbilitySystemComponent는 PawnExtensionComponent를 통해 처리되므로,
 * PlayerState에 ASC가 있는 경우(소버린)와 폰에 ASC가 있는 경우(AI) 모두 동일한 방식으로 동작합니다.
 */

UCLASS()
class RETRIEVE_API ARetrieveCharacter : public ACharacter, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ARetrieveCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	URetrieveAbilitySystemComponent* GetRetrieveAbilitySystemComponent() const;
	URetrievePawnExtensionComponent* GetPawnExtensionComponent() const { return PawnExtensionComponent; }
	
protected:
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;
	
	virtual void OnRep_Controller() override;
	virtual void OnRep_PlayerState() override;
	
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	
	virtual void InitializeAbilitySystem();
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|Pawn")
	TObjectPtr<const URetrievePawnData> DefaultPawnData;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<URetrievePawnExtensionComponent> PawnExtensionComponent;
};
