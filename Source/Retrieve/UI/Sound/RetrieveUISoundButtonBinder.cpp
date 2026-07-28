#include "UI/Sound/RetrieveUISoundButtonBinder.h"

#include "Components/Button.h"
#include "UI/Sound/RetrieveUISoundTypes.h"
#include "UI/VFX/RetrieveUIVFXWidget.h"

void URetrieveUISoundButtonBinder::Bind(URetrieveUIVFXWidget* InOwner, UButton* InButton)
{
	Unbind();

	if (!InOwner || !InButton)
	{
		return;
	}

	OwnerWidget = InOwner;
	BoundButton = InButton;

	InButton->OnHovered.AddUniqueDynamic(this, &ThisClass::HandleHovered);
	InButton->OnUnhovered.AddUniqueDynamic(this, &ThisClass::HandleUnhovered);
	InButton->OnPressed.AddUniqueDynamic(this, &ThisClass::HandlePressed);
	InButton->OnReleased.AddUniqueDynamic(this, &ThisClass::HandleReleased);
}

void URetrieveUISoundButtonBinder::Unbind()
{
	if (UButton* Button = BoundButton.Get())
	{
		Button->OnHovered.RemoveDynamic(this, &ThisClass::HandleHovered);
		Button->OnUnhovered.RemoveDynamic(this, &ThisClass::HandleUnhovered);
		Button->OnPressed.RemoveDynamic(this, &ThisClass::HandlePressed);
		Button->OnReleased.RemoveDynamic(this, &ThisClass::HandleReleased);
	}

	BoundButton.Reset();
	OwnerWidget.Reset();
}

bool URetrieveUISoundButtonBinder::IsBoundTo(const UButton* Button) const
{
	return Button != nullptr && BoundButton.Get() == Button;
}

void URetrieveUISoundButtonBinder::HandleHovered()
{
	if (URetrieveUIVFXWidget* Owner = OwnerWidget.Get())
	{
		Owner->PlayUISound(ERetrieveUISoundEvent::Hover);
	}
}

void URetrieveUISoundButtonBinder::HandleUnhovered()
{
	if (URetrieveUIVFXWidget* Owner = OwnerWidget.Get())
	{
		Owner->PlayUISound(ERetrieveUISoundEvent::Unhover);
	}
}

void URetrieveUISoundButtonBinder::HandlePressed()
{
	if (URetrieveUIVFXWidget* Owner = OwnerWidget.Get())
	{
		Owner->PlayUISound(ERetrieveUISoundEvent::Press);
	}
}

void URetrieveUISoundButtonBinder::HandleReleased()
{
	if (URetrieveUIVFXWidget* Owner = OwnerWidget.Get())
	{
		Owner->PlayUISound(ERetrieveUISoundEvent::Release);
	}
}
