#include "UI/VFX/RetrieveElementGaugeVFXWidget.h"

#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/RetrieveMessageTypes.h"

void URetrieveElementGaugeVFXWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UWorld* World = GetWorld())
	{
		ElementGaugeFullListener = UGameplayMessageSubsystem::Get(World).RegisterListener<FRetrieveElementGaugeFullPayload>(
			RetrieveGameplayTags::Channel_ElementGauge_Full,
			[this](FGameplayTag Channel, const FRetrieveElementGaugeFullPayload& Payload)
			{
				HandleElementGaugeFull(Channel, Payload);
			});
	}
}

void URetrieveElementGaugeVFXWidget::NativeDestruct()
{
	if (ElementGaugeFullListener.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			UGameplayMessageSubsystem::Get(World).UnregisterListener(ElementGaugeFullListener);
		}
		ElementGaugeFullListener = FGameplayMessageListenerHandle();
	}

	Super::NativeDestruct();
}

void URetrieveElementGaugeVFXWidget::HandleElementGaugeFull(FGameplayTag /*Channel*/, const FRetrieveElementGaugeFullPayload& Payload)
{
	PlayUIVFXOnWidget(RetrieveGameplayTags::UI_VFX_Gauge_FullPulse, ResolveGaugeVFXTarget());
	BP_OnElementGaugeFull(Payload);
}

UWidget* URetrieveElementGaugeVFXWidget::ResolveGaugeVFXTarget() const
{
	if (GaugeVFXTarget)
	{
		return GaugeVFXTarget;
	}

	return ResolveDefaultVFXTarget();
}
