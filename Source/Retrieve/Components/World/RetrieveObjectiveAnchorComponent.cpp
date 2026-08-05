#include "Components/World/RetrieveObjectiveAnchorComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Subsystems/RetrieveObjectiveMarkerSubsystem.h"

URetrieveObjectiveAnchorComponent::URetrieveObjectiveAnchorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

FVector URetrieveObjectiveAnchorComponent::GetMarkerWorldLocation() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor ? OwnerActor->GetActorLocation() + MarkerOffset : MarkerOffset;
}

void URetrieveObjectiveAnchorComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!ObjectiveStepTag.IsValid())
	{
		return; // 태그 미설정 앵커는 등록하지 않는다.
	}

	if (UWorld* World = GetWorld())
	{
		if (URetrieveObjectiveMarkerSubsystem* MarkerSub = World->GetSubsystem<URetrieveObjectiveMarkerSubsystem>())
		{
			MarkerSub->RegisterAnchor(this);
		}
	}
}

void URetrieveObjectiveAnchorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (URetrieveObjectiveMarkerSubsystem* MarkerSub = World->GetSubsystem<URetrieveObjectiveMarkerSubsystem>())
		{
			MarkerSub->UnregisterAnchor(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}
