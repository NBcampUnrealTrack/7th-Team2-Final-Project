#include "UI/RetrieveGamePanelWidget.h"

#include "GameplayTags/RetrieveGameplayTags.h"

URetrieveGamePanelWidget::URetrieveGamePanelWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(true);
}

void URetrieveGamePanelWidget::RequestClose()
{
	OnCloseRequested.Broadcast();
}

bool URetrieveGamePanelWidget::PlayPanelOpenVFX()
{
	return PlayUIVFX(RetrieveGameplayTags::UI_VFX_Panel_Open);
}

bool URetrieveGamePanelWidget::PlayPanelCloseVFX()
{
	return PlayUIVFX(RetrieveGameplayTags::UI_VFX_Panel_Close, true);
}

FReply URetrieveGamePanelWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	// Escape 처리는 PlayerController.InputKey에서 먼저 가로채므로 여기서는 ToggleKey만 담당
	if (InKeyEvent.GetKey() == ToggleKey)
	{
		RequestClose();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
