#pragma once

#include "Commandlets/Commandlet.h"
#include "RetrieveMigrateInteractionPresetsCommandlet.generated.h"

UCLASS()
class RETRIEVE_API URetrieveMigrateInteractionPresetsCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	URetrieveMigrateInteractionPresetsCommandlet();

	virtual int32 Main(const FString& Params) override;
};
