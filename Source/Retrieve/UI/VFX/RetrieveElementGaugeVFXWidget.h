#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "UI/VFX/RetrieveUIVFXWidget.h"
#include "RetrieveElementGaugeVFXWidget.generated.h"

class UWidget;

UCLASS(Abstract, Blueprintable)
class RETRIEVE_API URetrieveElementGaugeVFXWidget : public URetrieveUIVFXWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|UI VFX|Gauge")
	TObjectPtr<UWidget> GaugeVFXTarget;

	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|UI VFX|Gauge")
	void BP_OnElementGaugeFull(const FRetrieveElementGaugeFullPayload& Payload);

	void HandleElementGaugeFull(FGameplayTag Channel, const FRetrieveElementGaugeFullPayload& Payload);
	UWidget* ResolveGaugeVFXTarget() const;

private:
	FGameplayMessageListenerHandle ElementGaugeFullListener;
};
