#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "RetrieveCheatManager.generated.h"

class UAbilitySystemComponent;

/**
 * 
 */
UCLASS()
class RETRIEVE_API URetrieveCheatManager : public UCheatManager
{
	GENERATED_BODY()
	
public:
	UFUNCTION(Exec, Category = "Retrieve|Debug")
	void RetrieveKillPlayer();
	
	UFUNCTION(Exec, Category = "Retrieve|Debug")
	void RetrieveDamagePlayer(float Amount);
	
	UFUNCTION(Exec, Category = "Retrieve|Debug")
	void RetrieveSetHealth(float Value);
	
	UFUNCTION(Exec, Category = "Retrieve|Debug")
	void RetrieveTestGuardHit(bool bHeavy);

private:
	UAbilitySystemComponent* GetLocalPlayerASC() const;
};
