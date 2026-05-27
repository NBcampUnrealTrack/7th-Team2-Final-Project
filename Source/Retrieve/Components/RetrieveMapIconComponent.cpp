#include "Components/RetrieveMapIconComponent.h"
#include "Subsystems/RetrieveMapSubsystem.h"

URetrieveMapIconComponent::URetrieveMapIconComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URetrieveMapIconComponent::OnRegister()
{
	Super::OnRegister();
}

void URetrieveMapIconComponent::OnUnregister()
{
	UnregisterFromMapSubsystem();
	Super::OnUnregister();
}

void URetrieveMapIconComponent::BeginPlay()
{
	Super::BeginPlay();
	RegisterWithMapSubsystem();
}

void URetrieveMapIconComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromMapSubsystem();
	Super::EndPlay(EndPlayReason);
}

void URetrieveMapIconComponent::RegisterWithMapSubsystem()
{
	if (UWorld* World = GetWorld())
	{
		if (URetrieveMapSubsystem* Sub = World->GetSubsystem<URetrieveMapSubsystem>())
		{
			Sub->RegisterIcon(this);
		}
	}
}

void URetrieveMapIconComponent::UnregisterFromMapSubsystem()
{
	if (UWorld* World = GetWorld())
	{
		if (URetrieveMapSubsystem* Sub = World->GetSubsystem<URetrieveMapSubsystem>())
		{
			Sub->UnregisterIcon(this);
		}
	}
}
