#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "RetrieveFixBonfireBlueprintCommandlet.generated.h"

UCLASS()
class RETRIEVE_API URetrieveFixBonfireBlueprintCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	URetrieveFixBonfireBlueprintCommandlet();

	virtual int32 Main(const FString& Params) override;
};
