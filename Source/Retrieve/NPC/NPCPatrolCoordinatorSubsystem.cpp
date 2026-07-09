#include "NPC/NPCPatrolCoordinatorSubsystem.h"

#include "AIController.h"

bool UNPCPatrolCoordinatorSubsystem::IsPointTooCrowded(const FVector& Point, const AAIController* RequestingController,
                                                       float MinSeparation) const
{
	const float MinSeparationSq = MinSeparation * MinSeparation;
	for (const TPair<TWeakObjectPtr<const AAIController>, FVector>& Pair : ReservedDestinations)
	{
		const AAIController* Other = Pair.Key.Get();
		if (!Other || Other == RequestingController)
		{
			continue;
		}
		if (FVector::DistSquared(Point, Pair.Value) < MinSeparationSq)
		{
			return true;
		}
	}
	return false;
}

void UNPCPatrolCoordinatorSubsystem::ReserveDestination(const AAIController* Controller, const FVector& Point)
{
	if (Controller)
	{
		ReservedDestinations.Add(Controller, Point);
	}
}

void UNPCPatrolCoordinatorSubsystem::ReleaseDestination(const AAIController* Controller)
{
	if (Controller)
	{
		ReservedDestinations.Remove(Controller);
	}
}

void UNPCPatrolCoordinatorSubsystem::RegisterActivePatroller(AAIController* Controller)
{
	if (Controller)
	{
		ActivePatrollers.AddUnique(Controller);
	}
}

void UNPCPatrolCoordinatorSubsystem::UnregisterActivePatroller(const AAIController* Controller)
{
	ActivePatrollers.RemoveAll([Controller](const TWeakObjectPtr<AAIController>& WeakController)
	{
		return !WeakController.IsValid() || WeakController.Get() == Controller;
	});
}

AAIController* UNPCPatrolCoordinatorSubsystem::FindNearbyPatroller(const FVector& Location, float Radius,
                                                                   const AAIController* Requester) const
{
	const float RadiusSq = Radius * Radius;
	for (const TWeakObjectPtr<AAIController>& WeakController : ActivePatrollers)
	{
		AAIController* Other = WeakController.Get();
		if (!Other || Other == Requester)
		{
			continue;
		}
		const APawn* OtherPawn = Other->GetPawn();
		if (!OtherPawn)
		{
			continue;
		}
		if (FVector::DistSquared(Location, OtherPawn->GetActorLocation()) <= RadiusSq)
		{
			return Other;
		}
	}
	return nullptr;
}
