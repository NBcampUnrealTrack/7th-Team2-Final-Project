#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "UI/Sound/RetrieveUISoundTypes.h"
#include "RetrieveUISoundPreset.generated.h"

class USoundBase;

USTRUCT(BlueprintType)
struct RETRIEVE_API FRetrieveUIContextSound
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float VolumeMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float PitchMultiplier = 1.0f;
};

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

	/** Optional action-specific sounds layered on top of the role-based defaults. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound|Context",
		meta = (Categories = "UI.Sound"))
	TMap<FGameplayTag, FRetrieveUIContextSound> ContextSounds;

	USoundBase* GetSoundForEvent(ERetrieveUISoundEvent Event) const;
	const FRetrieveUIContextSound* FindContextSound(FGameplayTag ContextTag) const;
};
