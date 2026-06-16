#include "Components/World/RetrieveDialogueComponent.h"

#include "Character/LumenCharacter.h"
#include "Components/World/RetrieveInteractionResponseComponent.h"
#include "Core/RetrieveGameState.h"
#include "Player/RetrievePlayerController.h"

URetrieveDialogueComponent::URetrieveDialogueComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URetrieveDialogueComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!bAutoBindResponseComponent || bBoundToResponseComponent)
	{
		return;
	}
	if (AActor* Owner = GetOwner())
	{
		if (URetrieveInteractionResponseComponent* ResponseComponent =
			Owner->FindComponentByClass<URetrieveInteractionResponseComponent>())
		{
			ResponseComponent->OnApplied.AddDynamic(this, &URetrieveDialogueComponent::HandleInteract);
			bBoundToResponseComponent = true;
		}
	}
}

void URetrieveDialogueComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bBoundToResponseComponent)
	{
		if (AActor* Owner = GetOwner())
		{
			if (URetrieveInteractionResponseComponent* Response =
				Owner->FindComponentByClass<URetrieveInteractionResponseComponent>())
			{
				Response->OnApplied.RemoveDynamic(this, &URetrieveDialogueComponent::HandleInteract);
			}
		}
		bBoundToResponseComponent = false;
	}
	Super::EndPlay(EndPlayReason);
}

void URetrieveDialogueComponent::HandleInteract(AActor* Instigator)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	OpenConversationFor(Instigator);
}

void URetrieveDialogueComponent::OpenConversationFor(AActor* Instigator)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	APawn* InstigatorPawn = Cast<APawn>(Instigator);
	if (!InstigatorPawn)
	{
		return;
	}
	
	// TODO: 일반화하기. Cast를 IRetrieverConversationSpeaker로 교체할것.
	FText ResolvedSpeaker = SpeakerDisplayName;
	if (const ALumenCharacter* Lumen = Cast<ALumenCharacter>(GetOwner()))
	{
		if (!Lumen->DisplayName.IsEmpty())
		{
			ResolvedSpeaker = Lumen->DisplayName;
		}
	}

	if (UWorld* World = GetWorld())
	{
		if (ARetrieveGameState* GS = World->GetGameState<ARetrieveGameState>())
		{
			GS->SetActiveSpeaker(ResolvedSpeaker);
		}
	}

	if (ARetrievePlayerController* PC = Cast<ARetrievePlayerController>(InstigatorPawn->GetController()))
	{
		PC->Client_OpenConversation(GetOwner());
	}
}
