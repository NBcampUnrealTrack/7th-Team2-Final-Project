#include "World/RetrieveInteractDoor.h"

#include "Components/World/RetrieveInteractionResponseComponent.h"

ARetrieveInteractDoor::ARetrieveInteractDoor()
{
	InteractionResponse = CreateDefaultSubobject<URetrieveInteractionResponseComponent>(TEXT("InteractionResponse"));
}

void ARetrieveInteractDoor::BeginPlay()
{
	Super::BeginPlay();

	if (InteractionResponse)
	{
		InteractionResponse->OnApplied.AddDynamic(this, &ARetrieveInteractDoor::HandleInteracted);
	}
}

void ARetrieveInteractDoor::HandleInteracted(AActor* InteractionInstigator)
{
	ToggleDoor();
}
