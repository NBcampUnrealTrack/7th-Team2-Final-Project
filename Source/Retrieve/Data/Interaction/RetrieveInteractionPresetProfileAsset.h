#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RetrieveInteractionPresetProfileAsset.generated.h"

class UAnimMontage;
class URetrieveInteractionResultAsset;
class UTexture2D;
class UUserWidget;

USTRUCT(BlueprintType)
struct FRetrieveInteractionPresetData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Preset")
	FName PresetId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Preset|Prompt")
	FText DisplayText = INVTEXT("Interact");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Preset|Hold")
	bool bHoldInteraction = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Preset|Hold",
		meta = (EditCondition = "bHoldInteraction", ClampMin = "0.05"))
	float HoldDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Preset|Animation")
	TObjectPtr<UAnimMontage> InteractionMontage;

	/** 비주얼 메시(Synty 등 독립 AnimInstance)용 몽타주. 스켈레톤이 다를 때 별도 할당. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Preset|Animation")
	TObjectPtr<UAnimMontage> VisualMeshMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Preset|Animation",
		meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float MontagePlayRate = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Preset|Widget")
	TObjectPtr<UTexture2D> PromptIcon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Preset|Widget")
	FLinearColor PromptAccentColor = FLinearColor(0.78f, 0.63f, 0.13f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Preset|Widget")
	TSoftClassPtr<UUserWidget> WidgetClassOverride;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Preset|Widget|Manager Advanced")
	FName MgrProp_Icon = TEXT("InteractionIcon");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Preset|Widget|Manager Advanced")
	FName MgrProp_Color = TEXT("InteractionColor");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Preset|Widget|Manager Advanced")
	FName MgrProp_WidgetClass = TEXT("InteractionWidget");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Preset|Result")
	TArray<TObjectPtr<URetrieveInteractionResultAsset>> ResultAssets;
};

UCLASS(BlueprintType)
class RETRIEVE_API URetrieveInteractionPresetProfileAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction|Preset")
	TArray<FRetrieveInteractionPresetData> Presets;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Interaction")
	bool GetPreset(FName PresetId, FRetrieveInteractionPresetData& OutPreset) const;
};
