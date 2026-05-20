#include "Core/RetrieveGameMode.h"

#include "Player/RetrievePlayerController.h"
#include "Player/RetrievePlayerState.h"

ARetrieveGameMode::ARetrieveGameMode(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	PlayerControllerClass = ARetrievePlayerController::StaticClass();
	PlayerStateClass = ARetrievePlayerState::StaticClass();
}