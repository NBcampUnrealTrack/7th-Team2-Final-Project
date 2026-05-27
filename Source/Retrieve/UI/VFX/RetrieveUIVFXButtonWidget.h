#pragma once

#include "CoreMinimal.h"
#include "UI/VFX/RetrieveUIVFXWidget.h"
#include "RetrieveUIVFXButtonWidget.generated.h"

class UButton;
class UWidget;

UCLASS(Abstract, Blueprintable)
class RETRIEVE_API URetrieveUIVFXButtonWidget : public URetrieveUIVFXWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|UI VFX|Button", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Button_Target;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|UI VFX|Button")
	TObjectPtr<UWidget> ButtonVFXTarget;

	UFUNCTION()
	void HandleButtonHovered();

	UFUNCTION()
	void HandleButtonUnhovered();

	UFUNCTION()
	void HandleButtonPressed();

	UFUNCTION()
	void HandleButtonReleased();

	UWidget* ResolveButtonVFXTarget() const;
};
