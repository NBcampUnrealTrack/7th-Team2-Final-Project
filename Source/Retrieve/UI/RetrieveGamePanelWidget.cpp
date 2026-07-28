#include "UI/RetrieveGamePanelWidget.h"

#include "GameplayTags/RetrieveGameplayTags.h"
#include "UI/Sound/RetrieveUISoundTypes.h"

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
	const FGameplayTag ContextTag = GetPanelOpenSoundContext();
	if (ContextTag.IsValid())
	{
		PlayContextUISound(ContextTag, ERetrieveUISoundEvent::PanelOpen);
	}
	else
	{
		PlayUISound(ERetrieveUISoundEvent::PanelOpen);
	}
	return PlayUIVFX(RetrieveGameplayTags::UI_VFX_Panel_Open);
}

bool URetrieveGamePanelWidget::PlayPanelCloseVFX()
{
	const FGameplayTag ContextTag = GetPanelCloseSoundContext();
	if (ContextTag.IsValid())
	{
		PlayContextUISound(ContextTag, ERetrieveUISoundEvent::PanelClose);
	}
	else
	{
		PlayUISound(ERetrieveUISoundEvent::PanelClose);
	}
	return PlayUIVFX(RetrieveGameplayTags::UI_VFX_Panel_Close, true);
}

FReply URetrieveGamePanelWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey PressedKey = InKeyEvent.GetKey();

	// 패널이 열려 있을 때는 FInputModeUIOnly + 이 패널이 키보드 포커스를 가지므로
	// 키 입력이 PlayerController.InputKey가 아니라 여기로 먼저 들어온다.
	// 따라서 Escape도 이 지점에서 처리해야 인벤토리/월드맵/설정/상점이 ESC로 닫힌다.
	// (예전엔 "Escape는 PlayerController가 가로챈다"고 가정해 무시했으나, UI-Only 모드에선
	//  PlayerController.InputKey가 호출되지 않아 ESC가 아무 데서도 처리되지 않았다.)
	if (PressedKey == EKeys::Escape || (ToggleKey.IsValid() && PressedKey == ToggleKey))
	{
		RequestClose();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
