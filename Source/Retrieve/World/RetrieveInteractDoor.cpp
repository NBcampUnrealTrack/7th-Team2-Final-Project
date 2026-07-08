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
		InteractionResponse->OnApplied.AddUniqueDynamic(this, &ARetrieveInteractDoor::HandleInteracted);
	}
}

void ARetrieveInteractDoor::HandleInteracted(AActor* InteractionInstigator)
{
	ToggleDoor();
}
