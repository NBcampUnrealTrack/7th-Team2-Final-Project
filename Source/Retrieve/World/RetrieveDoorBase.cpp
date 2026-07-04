#include "World/RetrieveDoorBase.h"

#include "Net/UnrealNetwork.h"

ARetrieveDoorBase::ARetrieveDoorBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ARetrieveDoorBase::BeginPlay()
{
	Super::BeginPlay();

	bOpen = bStartOpen;
	ApplyDoorState(/*bInstant=*/true);
}

void ARetrieveDoorBase::OpenDoor()
{
	if (!HasAuthority() || bOpen)
	{
		return;
	}
	bOpen = true;
	ApplyDoorState(false);
}

void ARetrieveDoorBase::CloseDoor()
{
	if (!HasAuthority() || !bOpen)
	{
		return;
	}
	bOpen = false;
	ApplyDoorState(false);
}

void ARetrieveDoorBase::ToggleDoor()
{
	if (!HasAuthority())
	{
		return;
	}
	bOpen = !bOpen;
	ApplyDoorState(false);
}

void ARetrieveDoorBase::OnRep_bOpen()
{
	ApplyDoorState(false);
}

void ARetrieveDoorBase::ApplyDoorState(bool bInstant)
{
	if (bOpen)
	{
		OnDoorOpened(bInstant);
	}
	else
	{
		OnDoorClosed(bInstant);
	}
}

void ARetrieveDoorBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ARetrieveDoorBase, bOpen);
}
