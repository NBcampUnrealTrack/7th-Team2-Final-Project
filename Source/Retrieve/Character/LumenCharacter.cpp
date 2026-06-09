#include "Character/LumenCharacter.h"

#include "LumenFollowComponent.h"
#include "Components/RetrieveDialogueComponent.h"

ALumenCharacter::ALumenCharacter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	AutoReceiveInput = EAutoReceiveInput::Disabled;

	AIControllerClass = nullptr;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;

	FollowComponent = CreateDefaultSubobject<ULumenFollowComponent>(TEXT("FollowComponent"));
	DialogueComponent = CreateDefaultSubobject<URetrieveDialogueComponent>(TEXT("DialogueComponent"));
}
