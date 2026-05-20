#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "RetrieveGameMode.generated.h"

UCLASS()
class RETRIEVE_API ARetrieveGameMode : public AGameModeBase
{
	GENERATED_BODY()
	
public:
	ARetrieveGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
