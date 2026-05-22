#include "Character/LumenCharacter.h"

ALumenCharacter::ALumenCharacter(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	AutoReceiveInput = EAutoReceiveInput::Disabled;
	
	AIControllerClass = nullptr;
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	
	bUseControllerRotationYaw = false;
}
