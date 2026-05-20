#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "RetrievePlayerController.generated.h"

class ARetrievePlayerState;

UCLASS()
class RETRIEVE_API ARetrievePlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	ARetrievePlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	ARetrievePlayerState* GetRetrievePlayerState() const;
	
protected:
	// APlayerController interface
	virtual void PlayerTick(float DeltaTime) override;
};
