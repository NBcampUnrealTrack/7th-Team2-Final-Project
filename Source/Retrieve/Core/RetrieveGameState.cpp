#include "Core/RetrieveGameState.h"

#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameFramework/PlayerState.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "Net/UnrealNetwork.h"

ARetrieveGameState::ARetrieveGameState(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bReplicates = true;
}

void ARetrieveGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARetrieveGameState, SessionState);
	DOREPLIFETIME(ARetrieveGameState, HostPlayerState);
}

APawn* ARetrieveGameState::GetHostPawn() const
{
	return HostPlayerState ? HostPlayerState->GetPawn() : nullptr;
}

bool ARetrieveGameState::TransitionTo(ERetrieveSessionState NewState)
{
	if (!HasAuthority())
	{
		return false;
	}

	if (NewState == SessionState)
	{
		return false;
	}

	if (!IsLegalTransition(SessionState, NewState))
	{
		return false;
	}

	const ERetrieveSessionState Previous = SessionState;
	SessionState = NewState;
	BroadcastStateChange(Previous);

	return true;
}

void ARetrieveGameState::SetHostPlayerState(APlayerState* InPlayerState)
{
	if (!HasAuthority())
	{
		return;
	}

	if (HostPlayerState != nullptr || InPlayerState == nullptr)
	{
		return;
	}

	HostPlayerState = InPlayerState;
}

void ARetrieveGameState::OnRep_SessionState(ERetrieveSessionState Previous)
{
	BroadcastStateChange(Previous);
}

void ARetrieveGameState::BroadcastStateChange(ERetrieveSessionState Previous)
{
	FRetrieveSessionStatePayload Payload;
	Payload.PreviousState = Previous;
	Payload.NewState = SessionState;

	if (UWorld* World = GetWorld())
	{
		UGameplayMessageSubsystem::Get(World).BroadcastMessage(RetrieveGameplayTags::Channel_Session_StateChanged,
		                                                       Payload);
	}

	OnSessionStateChanged.Broadcast(Previous, SessionState);
}

bool ARetrieveGameState::IsLegalTransition(ERetrieveSessionState From, ERetrieveSessionState To)
{
	switch (From)
	{
		case ERetrieveSessionState::Loading:
			return To == ERetrieveSessionState::MainMenu;

		case ERetrieveSessionState::MainMenu:
			return To == ERetrieveSessionState::InGame;

		case ERetrieveSessionState::InGame:
			return To == ERetrieveSessionState::Result;

		case ERetrieveSessionState::Result:
			return To == ERetrieveSessionState::InGame || To == ERetrieveSessionState::MainMenu;

		default:
			return false;
	}
}
