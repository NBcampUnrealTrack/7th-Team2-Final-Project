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
	bBowAimToggleMode = false;

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

void URetrieveGameUserSettings::LoadSettings(bool bForceReload)
{
	Super::LoadSettings(bForceReload);
	ValidateRetrieveSettings();
}

void URetrieveGameUserSettings::SaveSettings()
{
	ValidateRetrieveSettings();
	Super::SaveSettings();
}

void URetrieveGameUserSettings::ResetCategoryToDefaults(ERetrieveSettingsCategory Category)
{
	// 전체 기본값을 산출할 임시 인스턴스. 해당 카테고리 필드만 여기서 복사한다.
	URetrieveGameUserSettings* Defaults =
		NewObject<URetrieveGameUserSettings>(GetTransientPackage(), URetrieveGameUserSettings::StaticClass());
	Defaults->SetToDefaults();

	switch (Category)
	{
	case ERetrieveSettingsCategory::Audio:
		MasterVolume = Defaults->MasterVolume;
		MusicVolume = Defaults->MusicVolume;
		SfxVolume = Defaults->SfxVolume;
		AmbienceVolume = Defaults->AmbienceVolume;
		UIVolume = Defaults->UIVolume;
		VoiceVolume = Defaults->VoiceVolume;
		bMuteWhenUnfocused = Defaults->bMuteWhenUnfocused;
		break;
	case ERetrieveSettingsCategory::Controls:
		MouseSensitivityX = Defaults->MouseSensitivityX;
		MouseSensitivityY = Defaults->MouseSensitivityY;
		bInvertMouseY = Defaults->bInvertMouseY;
		GamepadSensitivityX = Defaults->GamepadSensitivityX;
		GamepadSensitivityY = Defaults->GamepadSensitivityY;
		bGamepadVibration = Defaults->bGamepadVibration;
		bLockOnToggleMode = Defaults->bLockOnToggleMode;
		bBowAimToggleMode = Defaults->bBowAimToggleMode;
		break;
	case ERetrieveSettingsCategory::Gameplay:
		GameCulture = Defaults->GameCulture;
		bSubtitlesEnabled = Defaults->bSubtitlesEnabled;
		SubtitleTextScale = Defaults->SubtitleTextScale;
		FieldOfView = Defaults->FieldOfView;
		CameraShakeScale = Defaults->CameraShakeScale;
		bShowDamageNumbers = Defaults->bShowDamageNumbers;
		bTutorialHints = Defaults->bTutorialHints;
		break;
	case ERetrieveSettingsCategory::Accessibility:
		ColorBlindMode = Defaults->ColorBlindMode;
		ColorBlindStrength = Defaults->ColorBlindStrength;
		UITextScale = Defaults->UITextScale;
		bHighContrastHUD = Defaults->bHighContrastHUD;
		bReduceMotion = Defaults->bReduceMotion;
		bHoldToInteract = Defaults->bHoldToInteract;
		AimAssistStrength = Defaults->AimAssistStrength;
		SubtitleBackgroundOpacity = Defaults->SubtitleBackgroundOpacity;
		break;
	case ERetrieveSettingsCategory::Graphics:
		// 엔진 그래픽(해상도/창모드/VSync/프레임 제한/스케일러빌리티)만 기본값으로.
		// 다른 카테고리(오디오/조작/게임플레이/접근성)는 절대 건드리지 않는다.
		SetScreenResolution(Defaults->GetScreenResolution());
		SetFullscreenMode(Defaults->GetFullscreenMode());
		SetVSyncEnabled(Defaults->IsVSyncEnabled());
		SetFrameRateLimit(Defaults->GetFrameRateLimit());
		SetOverallScalabilityLevel(Defaults->GetOverallScalabilityLevel());
		GammaLevel = Defaults->GammaLevel;
		bMotionBlur = Defaults->bMotionBlur;
		break;
	default:
		break;
	}

	ValidateRetrieveSettings();
}

void URetrieveGameUserSettings::ValidateRetrieveSettings()
{
	auto Clamp01 = [](float& V) { V = FMath::Clamp(V, 0.f, 1.f); };
	Clamp01(MasterVolume);
	Clamp01(MusicVolume);
	Clamp01(SfxVolume);
	Clamp01(AmbienceVolume);
	Clamp01(UIVolume);
	Clamp01(VoiceVolume);

	MouseSensitivityX = FMath::Clamp(MouseSensitivityX, 0.1f, 3.f);
	MouseSensitivityY = FMath::Clamp(MouseSensitivityY, 0.1f, 3.f);
	GamepadSensitivityX = FMath::Clamp(GamepadSensitivityX, 0.1f, 3.f);
	GamepadSensitivityY = FMath::Clamp(GamepadSensitivityY, 0.1f, 3.f);

	SubtitleTextScale = FMath::Clamp(SubtitleTextScale, 0.5f, 2.f);
	FieldOfView = FMath::Clamp(FieldOfView, 60.f, 120.f);
	CameraShakeScale = FMath::Clamp(CameraShakeScale, 0.f, 1.f);

	ColorBlindStrength = FMath::Clamp(ColorBlindStrength, 0, 10);
	// HUD가 고정 배치라 과도한 배율은 요소가 겹친다. 안전 범위로 제한(겹침 방지).
	UITextScale = FMath::Clamp(UITextScale, 0.85f, 1.15f);
	AimAssistStrength = FMath::Clamp(AimAssistStrength, 0.f, 1.f);
	SubtitleBackgroundOpacity = FMath::Clamp(SubtitleBackgroundOpacity, 0.f, 1.f);

	GammaLevel = FMath::Clamp(GammaLevel, 1.0f, 4.0f);
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
