#include "Settings/RetrieveGameUserSettings.h"
#include "Engine/Engine.h"

URetrieveGameUserSettings::URetrieveGameUserSettings()
{
}

URetrieveGameUserSettings* URetrieveGameUserSettings::Get()
{
	if (GEngine)
	{
		return Cast<URetrieveGameUserSettings>(GEngine->GetGameUserSettings());
	}
	return nullptr;
}

void URetrieveGameUserSettings::SetToDefaults()
{
	Super::SetToDefaults();

	MasterVolume = 1.f;
	MusicVolume = 1.f;
	SfxVolume = 1.f;
	AmbienceVolume = 1.f;
	UIVolume = 1.f;
	VoiceVolume = 1.f;
	bMuteWhenUnfocused = true;

	MouseSensitivityX = 1.f;
	MouseSensitivityY = 1.f;
	bInvertMouseY = false;
	GamepadSensitivityX = 1.f;
	GamepadSensitivityY = 1.f;
	bGamepadVibration = true;
	bLockOnToggleMode = true;

	GameCulture.Reset();
	bSubtitlesEnabled = true;
	SubtitleTextScale = 1.f;
	FieldOfView = 90.f;
	CameraShakeScale = 1.f;
	bShowDamageNumbers = true;
	bTutorialHints = true;

	ColorBlindMode = ERetrieveColorBlindMode::Off;
	ColorBlindStrength = 10;
	UITextScale = 1.f;
	bHighContrastHUD = false;
	bReduceMotion = false;
	bHoldToInteract = false;
	AimAssistStrength = 0.f;
	SubtitleBackgroundOpacity = 0.5f;

	GammaLevel = 2.2f;
	bMotionBlur = true;
}

void URetrieveGameUserSettings::SetRetrieveWindowMode(ERetrieveWindowMode Mode)
{
	EWindowMode::Type EngineMode = EWindowMode::Fullscreen;
	switch (Mode)
	{
	case ERetrieveWindowMode::Fullscreen:         EngineMode = EWindowMode::Fullscreen; break;
	case ERetrieveWindowMode::WindowedFullscreen: EngineMode = EWindowMode::WindowedFullscreen; break;
	case ERetrieveWindowMode::Windowed:           EngineMode = EWindowMode::Windowed; break;
	default: break;
	}
	SetFullscreenMode(EngineMode);
}

ERetrieveWindowMode URetrieveGameUserSettings::GetRetrieveWindowMode() const
{
	switch (GetFullscreenMode())
	{
	case EWindowMode::Fullscreen:         return ERetrieveWindowMode::Fullscreen;
	case EWindowMode::WindowedFullscreen: return ERetrieveWindowMode::WindowedFullscreen;
	case EWindowMode::Windowed:           return ERetrieveWindowMode::Windowed;
	default:                              return ERetrieveWindowMode::Fullscreen;
	}
}
