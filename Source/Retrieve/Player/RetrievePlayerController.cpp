#include "Player/RetrievePlayerController.h"

#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "RetrievePlayerState.h"

ARetrievePlayerController::ARetrievePlayerController(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

ARetrievePlayerState* ARetrievePlayerController::GetRetrievePlayerState() const
{
	return CastChecked<ARetrievePlayerState>(PlayerState, ECastCheckedType::NullAllowed);
}

void ARetrievePlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	
	if (ARetrievePlayerState* RetrievePS = GetRetrievePlayerState())
	{
		if (URetrieveAbilitySystemComponent* RetrieveASC = RetrievePS->GetRetrieveAbilitySystemComponent())
		{
			const bool bGamePaused = IsPaused();
			RetrieveASC->ProcessAbilityInput(DeltaTime, bGamePaused);
		}
	}
}
