#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RetrieveUIVFXEditorUtility.generated.h"

UCLASS()
class RETRIEVE_API URetrieveUIVFXEditorUtility : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Retrieve|UI VFX|Editor")
	static bool ConfigureRecommendedUIVFXAssets();
};
