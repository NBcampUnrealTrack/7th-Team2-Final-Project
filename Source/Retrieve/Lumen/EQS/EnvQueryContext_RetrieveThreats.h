#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "EnvQueryContext_RetrieveThreats.generated.h"

UCLASS()
class RETRIEVE_API UEnvQueryContext_RetrieveThreats : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;

	UPROPERTY(EditDefaultsOnly, Category = "Retrieve")
	float GatherRadius = 1500.f;
};
