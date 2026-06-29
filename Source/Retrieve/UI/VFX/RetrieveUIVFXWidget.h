#pragma once

#include "CoreMinimal.h"
#include "UI/RetrieveElementAwareWidget.h"
#include "UI/Sound/RetrieveUISoundTypes.h"
#include "UI/VFX/RetrieveUIVFXTypes.h"
#include "RetrieveUIVFXWidget.generated.h"

class UButton;
class URetrieveUIVFXProfile;
class URetrieveUISoundPreset;
class UWidget;

USTRUCT()
struct FRetrieveActiveUIVFX
{
	GENERATED_BODY()

	UPROPERTY()
	TWeakObjectPtr<UWidget> TargetWidget;

	UPROPERTY()
	FRetrieveUIVFXPreset Preset;

	UPROPERTY()
	FGameplayTag RequestedEffectTag;

	UPROPERTY()
	float ElapsedTime = 0.0f;

	UPROPERTY()
	bool bNotifyWhenFinished = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRetrieveUIVFXFinishedSignature, FGameplayTag, EffectTag);

UCLASS(Abstract, Blueprintable)
class RETRIEVE_API URetrieveUIVFXWidget : public URetrieveElementAwareWidget
{
	GENERATED_BODY()

public:
	URetrieveUIVFXWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|UI VFX")
	TObjectPtr<URetrieveUIVFXProfile> VFXProfile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|UI VFX")
	TObjectPtr<UWidget> DefaultVFXTarget;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|UI Sound",
		meta = (DisplayName = "Override Sound Preset"))
	bool bOverrideSoundPreset = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|UI Sound",
		meta = (EditCondition = "bOverrideSoundPreset", DisplayName = "Sound Preset Override"))
	TObjectPtr<URetrieveUISoundPreset> SoundPresetOverride;

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|UI VFX")
	FRetrieveUIVFXFinishedSignature OnUIVFXFinished;

	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI VFX", meta = (Categories = "UI.VFX"))
	bool PlayUIVFX(FGameplayTag EffectTag, bool bNotifyWhenFinished = false);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI VFX", meta = (Categories = "UI.VFX"))
	bool PlayUIVFXOnWidget(FGameplayTag EffectTag, UWidget* TargetWidget, bool bNotifyWhenFinished = false);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI VFX")
	void StopUIVFX(UWidget* TargetWidget);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI VFX|Button")
	bool PlayButtonHoverVFX(UWidget* TargetWidget);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI VFX|Button")
	bool PlayButtonUnhoverVFX(UWidget* TargetWidget);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI VFX|Button")
	bool PlayButtonPressVFX(UWidget* TargetWidget);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI VFX|Button")
	bool PlayButtonReleaseVFX(UWidget* TargetWidget);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI VFX|Common")
	bool PlayTabSwitchVFX(UWidget* TargetWidget);

protected:
	friend class URetrieveUISoundButtonBinder;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void PlayUISound(ERetrieveUISoundEvent Event) const;
	void RegisterSoundButton(UButton* Button);

	UWidget* ResolveDefaultVFXTarget() const;
	void ApplyPresetAtAlpha(UWidget* TargetWidget, const FRetrieveUIVFXPreset& Preset, float Alpha) const;

private:
	UPROPERTY()
	TArray<FRetrieveActiveUIVFX> ActiveVFX;

	UPROPERTY()
	TArray<TObjectPtr<UObject>> SoundButtonBinders;
};
