
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Data/RetrieveDataTableTypes.h"
#include "AttackComboDefinition.generated.h"

/**
 * Anim Layer의 Element에 해당하는 Combo Attack Montage의 데이터를 관리하는 DataAsset
 */
UCLASS(BlueprintType)
class RETRIEVE_API UAttackComboDefinition : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FAttackComboVariant DefaultVariant;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FAttackComboVariant> ElementVariants;
	
	const FAttackComboVariant* ResolveVariant(const FGameplayTag& ElementTag) const;
};
