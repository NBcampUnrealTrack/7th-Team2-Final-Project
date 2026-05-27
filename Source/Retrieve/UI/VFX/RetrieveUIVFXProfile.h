#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UI/VFX/RetrieveUIVFXTypes.h"
#include "RetrieveUIVFXProfile.generated.h"

UCLASS(BlueprintType)
class RETRIEVE_API URetrieveUIVFXProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|UI VFX")
	TArray<FRetrieveUIVFXPreset> Presets;

	UFUNCTION(BlueprintPure, Category = "Retrieve|UI VFX", meta = (Categories = "UI.VFX"))
	bool GetPreset(FGameplayTag EffectTag, FRetrieveUIVFXPreset& OutPreset) const;
};
