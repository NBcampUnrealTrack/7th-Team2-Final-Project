#include "EnvQueryContext_RetrieveHost.h"

#include "Core/RetrieveGameState.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"

void UEnvQueryContext_RetrieveHost::ProvideContext(FEnvQueryInstance& QueryInstance,
                                                   FEnvQueryContextData& ContextData) const
{
	AActor* QuerierActor = Cast<AActor>(QueryInstance.Owner.Get());
	UWorld* World = QuerierActor ? QuerierActor->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}
	if (ARetrieveGameState* GS = World->GetGameState<ARetrieveGameState>())
	{
		if (AActor* Host = GS->GetHostPawn())
		{
			UEnvQueryItemType_Actor::SetContextHelper(ContextData, Host);
		}
	}
}
