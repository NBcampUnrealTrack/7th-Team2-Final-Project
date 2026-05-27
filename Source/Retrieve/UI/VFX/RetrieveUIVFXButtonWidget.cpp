#include "UI/VFX/RetrieveUIVFXButtonWidget.h"

#include "Components/Button.h"
#include "GameplayTags/RetrieveGameplayTags.h"

void URetrieveUIVFXButtonWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Target)
	{
		Button_Target->OnHovered.RemoveDynamic(this, &ThisClass::HandleButtonHovered);
		Button_Target->OnHovered.AddDynamic(this, &ThisClass::HandleButtonHovered);

		Button_Target->OnUnhovered.RemoveDynamic(this, &ThisClass::HandleButtonUnhovered);
		Button_Target->OnUnhovered.AddDynamic(this, &ThisClass::HandleButtonUnhovered);

		Button_Target->OnPressed.RemoveDynamic(this, &ThisClass::HandleButtonPressed);
		Button_Target->OnPressed.AddDynamic(this, &ThisClass::HandleButtonPressed);

		Button_Target->OnReleased.RemoveDynamic(this, &ThisClass::HandleButtonReleased);
		Button_Target->OnReleased.AddDynamic(this, &ThisClass::HandleButtonReleased);
	}
}

void URetrieveUIVFXButtonWidget::NativeDestruct()
{
	if (Button_Target)
	{
		Button_Target->OnHovered.RemoveDynamic(this, &ThisClass::HandleButtonHovered);
		Button_Target->OnUnhovered.RemoveDynamic(this, &ThisClass::HandleButtonUnhovered);
		Button_Target->OnPressed.RemoveDynamic(this, &ThisClass::HandleButtonPressed);
		Button_Target->OnReleased.RemoveDynamic(this, &ThisClass::HandleButtonReleased);
	}

	Super::NativeDestruct();
}

void URetrieveUIVFXButtonWidget::HandleButtonHovered()
{
	PlayUIVFXOnWidget(RetrieveGameplayTags::UI_VFX_Button_Hover, ResolveButtonVFXTarget());
}

void URetrieveUIVFXButtonWidget::HandleButtonUnhovered()
{
	PlayUIVFXOnWidget(RetrieveGameplayTags::UI_VFX_Button_Unhover, ResolveButtonVFXTarget());
}

void URetrieveUIVFXButtonWidget::HandleButtonPressed()
{
	PlayUIVFXOnWidget(RetrieveGameplayTags::UI_VFX_Button_Press, ResolveButtonVFXTarget());
}

void URetrieveUIVFXButtonWidget::HandleButtonReleased()
{
	PlayUIVFXOnWidget(RetrieveGameplayTags::UI_VFX_Button_Release, ResolveButtonVFXTarget());
}

UWidget* URetrieveUIVFXButtonWidget::ResolveButtonVFXTarget() const
{
	if (ButtonVFXTarget)
	{
		return ButtonVFXTarget;
	}

	return ResolveDefaultVFXTarget();
}
