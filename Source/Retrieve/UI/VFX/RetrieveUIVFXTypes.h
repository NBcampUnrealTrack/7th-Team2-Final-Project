#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RetrieveUIVFXTypes.generated.h"

class UCurveFloat;

USTRUCT(BlueprintType)
struct FRetrieveUIVFXPreset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|UI VFX", meta = (Categories = "UI.VFX"))
	FGameplayTag EffectTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|UI VFX")
	FName EffectName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|UI VFX")
	TObjectPtr<UCurveFloat> Curve = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|UI VFX", meta = (ClampMin = "0.0"))
	float Duration = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|UI VFX")
	FVector2D StartTranslation = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|UI VFX")
	FVector2D EndTranslation = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|UI VFX")
	FVector2D StartScale = FVector2D(1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|UI VFX")
	FVector2D EndScale = FVector2D(1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|UI VFX", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StartOpacity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|UI VFX", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EndOpacity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|UI VFX")
	FLinearColor FlashColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|UI VFX", meta = (ClampMin = "0.0"))
	float FlashIntensity = 1.0f;
};
