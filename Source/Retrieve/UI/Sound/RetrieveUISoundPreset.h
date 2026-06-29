#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UI/Sound/RetrieveUISoundTypes.h"
#include "RetrieveUISoundPreset.generated.h"

class USoundBase;

UCLASS(BlueprintType)
class RETRIEVE_API URetrieveUISoundPreset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound|Button")
	TObjectPtr<USoundBase> HoverSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound|Button")
	TObjectPtr<USoundBase> UnhoverSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound|Button")
	TObjectPtr<USoundBase> PressSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound|Button")
	TObjectPtr<USoundBase> ReleaseSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound|Panel")
	TObjectPtr<USoundBase> PanelOpenSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound|Panel")
	TObjectPtr<USoundBase> PanelCloseSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound|Navigation")
	TObjectPtr<USoundBase> TabSwitchSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound|Volume",
		meta = (ClampMin = 0.0f, ClampMax = 2.0f))
	float VolumeMultiplier = 1.0f;

	USoundBase* GetSoundForEvent(ERetrieveUISoundEvent Event) const;
};
