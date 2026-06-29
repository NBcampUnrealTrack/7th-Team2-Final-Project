#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RetrieveUISoundRegistry.generated.h"

class URetrieveUIVFXWidget;
class URetrieveUISoundPreset;

UCLASS(BlueprintType)
class RETRIEVE_API URetrieveUISoundRegistry : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Registry")
	TMap<TSubclassOf<URetrieveUIVFXWidget>, TObjectPtr<URetrieveUISoundPreset>> ClassPresets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Registry")
	TObjectPtr<URetrieveUISoundPreset> FallbackPreset;
};
