#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RetrieveBossHPBarEditorUtility.generated.h"

class UWidget;
class UWidgetBlueprint;
class UMVVMEditorSubsystem;
struct FMVVMBlueprintPropertyPath;

UCLASS()
class RETRIEVE_API URetrieveBossHPBarEditorUtility : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Retrieve|UI|Editor")
	static bool ConfigureBossHPBarMVVMBindings(
		const FString& AssetPath = TEXT("/Game/Retrieve/UI/HUD/WBP_BossHPBar_SourceStyle"));

private:
#if WITH_EDITOR
	static FMVVMBlueprintPropertyPath MakeViewModelPath(const UWidgetBlueprint* WidgetBlueprint, FGuid ViewModelId, FName FunctionName);
	static FMVVMBlueprintPropertyPath MakeWidgetPropertyPath(const UWidgetBlueprint* WidgetBlueprint, UWidget* Widget, FName PropertyName);
	static bool AddOneWayBinding(UMVVMEditorSubsystem* EditorSubsystem, UWidgetBlueprint* WidgetBlueprint, FGuid ViewModelId, FName SourceFunctionName, UWidget* DestinationWidget, FName DestinationPropertyName);
#endif
};
