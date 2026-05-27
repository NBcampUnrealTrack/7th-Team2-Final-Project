#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "UI/VFX/RetrieveUIVFXTypes.h"
#include "RetrieveUIVFXWidget.generated.h"

class URetrieveUIVFXProfile;
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
class RETRIEVE_API URetrieveUIVFXWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	URetrieveUIVFXWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|UI VFX")
	TObjectPtr<URetrieveUIVFXProfile> VFXProfile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|UI VFX")
	TObjectPtr<UWidget> DefaultVFXTarget;

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
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UWidget* ResolveDefaultVFXTarget() const;
	void ApplyPresetAtAlpha(UWidget* TargetWidget, const FRetrieveUIVFXPreset& Preset, float Alpha) const;

private:
	UPROPERTY()
	TArray<FRetrieveActiveUIVFX> ActiveVFX;
};
