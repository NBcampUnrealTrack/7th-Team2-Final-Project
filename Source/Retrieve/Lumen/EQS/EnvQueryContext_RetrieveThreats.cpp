#include "EnvQueryContext_RetrieveThreats.h"

#include "EngineUtils.h"
#include "GenericTeamAgentInterface.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"

void UEnvQueryContext_RetrieveThreats::ProvideContext(FEnvQueryInstance& QueryInstance,
                                                      FEnvQueryContextData& ContextData) const
{
	APawn* QuerierPawn = Cast<APawn>(QueryInstance.Owner.Get());
	if (!QuerierPawn)
	{
		return;
	}

	const IGenericTeamAgentInterface* TeamAgent = Cast<IGenericTeamAgentInterface>(QuerierPawn->GetController());
	UWorld* World = QuerierPawn->GetWorld();
	if (!TeamAgent || !World)
	{
		return;
	}

	const FVector Origin = QuerierPawn->GetActorLocation();
	const float RadiusSq = FMath::Square(GatherRadius);

	// TODO: 이벤트당 1번이라 빈도는 낮지만, 오버랩 쿼리로 최적화 고려해볼것
	TArray<AActor*> Threats;
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* Other = *It;
		if (!Other || Other == QuerierPawn)
		{
			continue;
		}
		if (FVector::DistSquared(Origin, Other->GetActorLocation()) > RadiusSq)
		{
			continue;
		}
		if (TeamAgent->GetTeamAttitudeTowards(*Other) == ETeamAttitude::Hostile)
		{
			Threats.Add(Other);
		}
	}

	UEnvQueryItemType_Actor::SetContextHelper(ContextData, Threats);
}
