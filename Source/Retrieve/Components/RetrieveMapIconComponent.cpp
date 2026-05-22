#include "Components/RetrieveMapIconComponent.h"
#include "Subsystems/RetrieveMapSubsystem.h"

URetrieveMapIconComponent::URetrieveMapIconComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URetrieveMapIconComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		if (URetrieveMapSubsystem* Sub = World->GetSubsystem<URetrieveMapSubsystem>())
		{
			Sub->RegisterIcon(this);
		}
	}
}

void URetrieveMapIconComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (URetrieveMapSubsystem* Sub = World->GetSubsystem<URetrieveMapSubsystem>())
		{
			Sub->UnregisterIcon(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}
