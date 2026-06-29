#include "UI/Sound/RetrieveUISoundPreset.h"

#include "Sound/SoundBase.h"

USoundBase* URetrieveUISoundPreset::GetSoundForEvent(ERetrieveUISoundEvent Event) const
{
	switch (Event)
	{
	case ERetrieveUISoundEvent::Hover:
		return HoverSound;
	case ERetrieveUISoundEvent::Unhover:
		return UnhoverSound;
	case ERetrieveUISoundEvent::Press:
		return PressSound;
	case ERetrieveUISoundEvent::Release:
		return ReleaseSound;
	case ERetrieveUISoundEvent::PanelOpen:
		return PanelOpenSound;
	case ERetrieveUISoundEvent::PanelClose:
		return PanelCloseSound;
	case ERetrieveUISoundEvent::TabSwitch:
		return TabSwitchSound;
	default:
		return nullptr;
	}
}
