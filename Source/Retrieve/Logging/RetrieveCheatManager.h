#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CheatManager.h"
#include "RetrieveCheatManager.generated.h"

class UAbilitySystemComponent;
class ARetrieveGameState;

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

	UFUNCTION(Exec, Category = "Retrieve|Debug")
	void RetrieveQuestComplete(const FString& StepTagName);

	UFUNCTION(Exec, Category = "Retrieve|Debug")
	void RetrieveLumenToggleWait();

	UFUNCTION(Exec, Category = "Retrieve|Debug")
	void RetrieveLumenRecall();

	UFUNCTION(Exec, Category = "Retrieve|Debug")
	void RetrieveToggleCombatTag();

private:
	UAbilitySystemComponent* GetLocalPlayerASC() const;
	ARetrieveGameState* GetRetrieveGameState() const;
};
