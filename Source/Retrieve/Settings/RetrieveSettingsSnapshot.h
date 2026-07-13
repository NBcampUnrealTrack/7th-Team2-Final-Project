#pragma once

#include "CoreMinimal.h"
#include "Settings/RetrieveGameUserSettings.h"

/**
 * 설정 화면을 열 때의 확정값을 통째로 복사해 두는 스냅샷(baseline).
 * Apply 없이 화면을 닫으면 이 값으로 되돌려 라이브 프리뷰로 바뀐 런타임을 원복한다.
 * (엔진 그래픽 + 프로젝트 전용 설정 전부 포함)
 *
 * 순수 C++ 구조체다(BP 노출·직렬화 없음). 설정 위젯 내부에서만 사용한다.
 */
struct FRetrieveSettingsSnapshot
{
	// ── 엔진 그래픽 ──────────────────────────────────────────────
	FIntPoint Resolution = FIntPoint(1920, 1080);
	int32 WindowMode = 0;
	bool bVSync = false;
	float FrameRateLimit = 0.f;
	int32 OverallQuality = 3;
	int32 ShadowQuality = 3;
	int32 TextureQuality = 3;
	int32 EffectQuality = 3;

	// ── 프로젝트 전용 ────────────────────────────────────────────
	float MasterVolume = 1.f;
	float MusicVolume = 1.f;
	float SfxVolume = 1.f;
	float AmbienceVolume = 1.f;
	float UIVolume = 1.f;
	float VoiceVolume = 1.f;
	bool bMuteWhenUnfocused = true;

	float MouseSensitivityX = 1.f;
	float MouseSensitivityY = 1.f;
	bool bInvertMouseY = false;
	float GamepadSensitivityX = 1.f;
	float GamepadSensitivityY = 1.f;
	bool bGamepadVibration = true;
	bool bLockOnToggleMode = true;

	FString GameCulture;
	bool bSubtitlesEnabled = true;
	float SubtitleTextScale = 1.f;
	float FieldOfView = 90.f;
	float CameraShakeScale = 1.f;
	bool bShowDamageNumbers = true;
	bool bTutorialHints = true;

	ERetrieveColorBlindMode ColorBlindMode = ERetrieveColorBlindMode::Off;
	int32 ColorBlindStrength = 10;
	float UITextScale = 1.f;
	bool bHighContrastHUD = false;
	bool bReduceMotion = false;
	bool bHideHUD = false;
	bool bHoldToInteract = false;
	float AimAssistStrength = 0.f;
	float SubtitleBackgroundOpacity = 0.5f;

	float GammaLevel = 2.2f;
	bool bMotionBlur = true;

	/** 현재 확정값을 스냅샷으로 복사한다. */
	void CaptureFrom(const URetrieveGameUserSettings* S)
	{
		if (!S) { return; }

		Resolution = S->GetScreenResolution();
		WindowMode = static_cast<int32>(S->GetFullscreenMode());
		bVSync = S->IsVSyncEnabled();
		FrameRateLimit = S->GetFrameRateLimit();
		OverallQuality = S->GetOverallScalabilityLevel();
		ShadowQuality = S->GetShadowQuality();
		TextureQuality = S->GetTextureQuality();
		EffectQuality = S->GetVisualEffectQuality();

		MasterVolume = S->MasterVolume;
		MusicVolume = S->MusicVolume;
		SfxVolume = S->SfxVolume;
		AmbienceVolume = S->AmbienceVolume;
		UIVolume = S->UIVolume;
		VoiceVolume = S->VoiceVolume;
		bMuteWhenUnfocused = S->bMuteWhenUnfocused;

		MouseSensitivityX = S->MouseSensitivityX;
		MouseSensitivityY = S->MouseSensitivityY;
		bInvertMouseY = S->bInvertMouseY;
		GamepadSensitivityX = S->GamepadSensitivityX;
		GamepadSensitivityY = S->GamepadSensitivityY;
		bGamepadVibration = S->bGamepadVibration;
		bLockOnToggleMode = S->bLockOnToggleMode;

		GameCulture = S->GameCulture;
		bSubtitlesEnabled = S->bSubtitlesEnabled;
		SubtitleTextScale = S->SubtitleTextScale;
		FieldOfView = S->FieldOfView;
		CameraShakeScale = S->CameraShakeScale;
		bShowDamageNumbers = S->bShowDamageNumbers;
		bTutorialHints = S->bTutorialHints;

		ColorBlindMode = S->ColorBlindMode;
		ColorBlindStrength = S->ColorBlindStrength;
		UITextScale = S->UITextScale;
		bHighContrastHUD = S->bHighContrastHUD;
		bReduceMotion = S->bReduceMotion;
		bHideHUD = S->bHideHUD;
		bHoldToInteract = S->bHoldToInteract;
		AimAssistStrength = S->AimAssistStrength;
		SubtitleBackgroundOpacity = S->SubtitleBackgroundOpacity;

		GammaLevel = S->GammaLevel;
		bMotionBlur = S->bMotionBlur;
	}

	/** 스냅샷 값을 설정 객체에 되돌린다(런타임 재적용/저장은 호출자가 수행). */
	void RestoreTo(URetrieveGameUserSettings* S) const
	{
		if (!S) { return; }

		S->SetScreenResolution(Resolution);
		S->SetFullscreenMode(static_cast<EWindowMode::Type>(WindowMode));
		S->SetVSyncEnabled(bVSync);
		S->SetFrameRateLimit(FrameRateLimit);
		if (OverallQuality >= 0)
		{
			S->SetOverallScalabilityLevel(OverallQuality);
		}
		S->SetShadowQuality(ShadowQuality);
		S->SetTextureQuality(TextureQuality);
		S->SetVisualEffectQuality(EffectQuality);

		S->MasterVolume = MasterVolume;
		S->MusicVolume = MusicVolume;
		S->SfxVolume = SfxVolume;
		S->AmbienceVolume = AmbienceVolume;
		S->UIVolume = UIVolume;
		S->VoiceVolume = VoiceVolume;
		S->bMuteWhenUnfocused = bMuteWhenUnfocused;

		S->MouseSensitivityX = MouseSensitivityX;
		S->MouseSensitivityY = MouseSensitivityY;
		S->bInvertMouseY = bInvertMouseY;
		S->GamepadSensitivityX = GamepadSensitivityX;
		S->GamepadSensitivityY = GamepadSensitivityY;
		S->bGamepadVibration = bGamepadVibration;
		S->bLockOnToggleMode = bLockOnToggleMode;

		S->GameCulture = GameCulture;
		S->bSubtitlesEnabled = bSubtitlesEnabled;
		S->SubtitleTextScale = SubtitleTextScale;
		S->FieldOfView = FieldOfView;
		S->CameraShakeScale = CameraShakeScale;
		S->bShowDamageNumbers = bShowDamageNumbers;
		S->bTutorialHints = bTutorialHints;

		S->ColorBlindMode = ColorBlindMode;
		S->ColorBlindStrength = ColorBlindStrength;
		S->UITextScale = UITextScale;
		S->bHighContrastHUD = bHighContrastHUD;
		S->bReduceMotion = bReduceMotion;
		S->bHideHUD = bHideHUD;
		S->bHoldToInteract = bHoldToInteract;
		S->AimAssistStrength = AimAssistStrength;
		S->SubtitleBackgroundOpacity = SubtitleBackgroundOpacity;

		S->GammaLevel = GammaLevel;
		S->bMotionBlur = bMotionBlur;
	}

	/** 화면 해상도/창 모드가 다른지(해상도 확인 대상 여부). */
	bool DiffersInDisplayMode(const FRetrieveSettingsSnapshot& Other) const
	{
		return Resolution != Other.Resolution || WindowMode != Other.WindowMode;
	}
};
