#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "RetrieveDataTableTypes.generated.h"

USTRUCT(BlueprintType)
struct RETRIEVE_API FCharacterStats : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Stats")
	float MaxHealth = 100.0f;
};
