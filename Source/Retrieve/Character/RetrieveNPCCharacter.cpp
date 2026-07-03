#include "Character/RetrieveNPCCharacter.h"

#include "Components/World/RetrieveDialogueComponent.h"

ARetrieveNPCCharacter::ARetrieveNPCCharacter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	AutoReceiveInput = EAutoReceiveInput::Disabled;

	AIControllerClass = nullptr;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	bUseControllerRotationYaw = false;

	DialogueComponent = CreateDefaultSubobject<URetrieveDialogueComponent>(TEXT("DialogueComponent"));
}
