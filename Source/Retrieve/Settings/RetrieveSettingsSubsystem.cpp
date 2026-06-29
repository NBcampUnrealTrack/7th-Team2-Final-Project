#include "Settings/RetrieveSettingsSubsystem.h"
#include "Settings/RetrieveGameUserSettings.h"
#include "Settings/RetrieveSettingsConfig.h"

#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetInternationalizationLibrary.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundClass.h"
#include "HAL/IConsoleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/App.h"
#include "Rendering/SlateRenderer.h"
#include "Rendering/RenderingCommon.h"
#include "AudioDevice.h"
#include "AudioDeviceHandle.h"

void URetrieveSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 부팅 시 저장된 설정을 1회 적용(저장은 다시 하지 않음).
	ApplyAllSettings(/*bSaveToDisk*/ false);
}

URetrieveSettingsSubsystem* URetrieveSettingsSubsystem::Get(const UObject* WorldContextObject)
{
	if (!GEngine)
	{
		return nullptr;
	}
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	if (!World)
	{
		return nullptr;
	}
	if (const APlayerController* PC = World->GetFirstPlayerController())
	{
		if (ULocalPlayer* LP = PC->GetLocalPlayer())
		{
			return LP->GetSubsystem<URetrieveSettingsSubsystem>();
		}
	}
	return nullptr;
}

URetrieveGameUserSettings* URetrieveSettingsSubsystem::GetSettings() const
{
	return URetrieveGameUserSettings::Get();
}

UWorld* URetrieveSettingsSubsystem::GetSubsystemWorld() const
{
	if (const ULocalPlayer* LP = GetLocalPlayer())
	{
		return LP->GetWorld();
	}
	return nullptr;
}

void URetrieveSettingsSubsystem::ApplyAllSettings(bool bSaveToDisk)
{
	URetrieveGameUserSettings* S = GetSettings();
	if (!S)
	{
		return;
	}

	ApplyGraphics(S);
	ApplyAudio(S);
	ApplyControls(S);
	ApplyGameplay(S);
	ApplyAccessibility(S);

	if (bSaveToDisk)
	{
		S->SaveSettings();
	}

	OnSettingChanged.Broadcast(ERetrieveSettingsCategory::MAX);
}

void URetrieveSettingsSubsystem::ApplyCategory(ERetrieveSettingsCategory Category, bool bSaveToDisk)
{
	URetrieveGameUserSettings* S = GetSettings();
	if (!S)
	{
		return;
	}

	switch (Category)
	{
	case ERetrieveSettingsCategory::Graphics:      ApplyGraphics(S, bSaveToDisk); break;
	case ERetrieveSettingsCategory::Controls:      ApplyControls(S); break;
	case ERetrieveSettingsCategory::Audio:         ApplyAudio(S); break;
	case ERetrieveSettingsCategory::Gameplay:      ApplyGameplay(S); break;
	case ERetrieveSettingsCategory::Accessibility: ApplyAccessibility(S); break;
	default: break;
	}

	if (bSaveToDisk)
	{
		S->SaveSettings();
	}

	OnSettingChanged.Broadcast(Category);
}

void URetrieveSettingsSubsystem::ResetCategory(ERetrieveSettingsCategory Category)
{
	URetrieveGameUserSettings* S = GetSettings();
	if (!S)
	{
		return;
	}

	// 단순화를 위해 전체 기본값 산출 후 해당 카테고리 필드만 복사한다.
	const TObjectPtr<URetrieveGameUserSettings> Defaults =
		NewObject<URetrieveGameUserSettings>(GetTransientPackage(), URetrieveGameUserSettings::StaticClass());
	Defaults->SetToDefaults();

	switch (Category)
	{
	case ERetrieveSettingsCategory::Audio:
		S->MasterVolume = Defaults->MasterVolume;
		S->MusicVolume = Defaults->MusicVolume;
		S->SfxVolume = Defaults->SfxVolume;
		S->AmbienceVolume = Defaults->AmbienceVolume;
		S->UIVolume = Defaults->UIVolume;
		S->VoiceVolume = Defaults->VoiceVolume;
		S->bMuteWhenUnfocused = Defaults->bMuteWhenUnfocused;
		break;
	case ERetrieveSettingsCategory::Controls:
		S->MouseSensitivityX = Defaults->MouseSensitivityX;
		S->MouseSensitivityY = Defaults->MouseSensitivityY;
		S->bInvertMouseY = Defaults->bInvertMouseY;
		S->GamepadSensitivityX = Defaults->GamepadSensitivityX;
		S->GamepadSensitivityY = Defaults->GamepadSensitivityY;
		S->bGamepadVibration = Defaults->bGamepadVibration;
		S->bLockOnToggleMode = Defaults->bLockOnToggleMode;
		break;
	case ERetrieveSettingsCategory::Gameplay:
		S->GameCulture = Defaults->GameCulture;
		S->bSubtitlesEnabled = Defaults->bSubtitlesEnabled;
		S->SubtitleTextScale = Defaults->SubtitleTextScale;
		S->FieldOfView = Defaults->FieldOfView;
		S->CameraShakeScale = Defaults->CameraShakeScale;
		S->bShowDamageNumbers = Defaults->bShowDamageNumbers;
		S->bTutorialHints = Defaults->bTutorialHints;
		break;
	case ERetrieveSettingsCategory::Accessibility:
		S->ColorBlindMode = Defaults->ColorBlindMode;
		S->ColorBlindStrength = Defaults->ColorBlindStrength;
		S->UITextScale = Defaults->UITextScale;
		S->bHighContrastHUD = Defaults->bHighContrastHUD;
		S->bReduceMotion = Defaults->bReduceMotion;
		S->bHoldToInteract = Defaults->bHoldToInteract;
		S->AimAssistStrength = Defaults->AimAssistStrength;
		S->SubtitleBackgroundOpacity = Defaults->SubtitleBackgroundOpacity;
		break;
	case ERetrieveSettingsCategory::Graphics:
		S->GammaLevel = Defaults->GammaLevel;
		S->bMotionBlur = Defaults->bMotionBlur;
		S->SetToDefaults(); // 해상도/스케일러빌리티 기본값 포함
		break;
	default:
		break;
	}

	ApplyCategory(Category, /*bSaveToDisk*/ true);
}

void URetrieveSettingsSubsystem::SaveSettings()
{
	if (URetrieveGameUserSettings* S = GetSettings())
	{
		S->SaveSettings();
	}
}

void URetrieveSettingsSubsystem::SetChannelVolume(ERetrieveAudioChannel Channel, float Volume, bool bApplyNow)
{
	URetrieveGameUserSettings* S = GetSettings();
	if (!S)
	{
		return;
	}
	Volume = FMath::Clamp(Volume, 0.f, 1.f);
	switch (Channel)
	{
	case ERetrieveAudioChannel::Master:   S->MasterVolume = Volume; break;
	case ERetrieveAudioChannel::Music:    S->MusicVolume = Volume; break;
	case ERetrieveAudioChannel::Sfx:      S->SfxVolume = Volume; break;
	case ERetrieveAudioChannel::Ambience: S->AmbienceVolume = Volume; break;
	case ERetrieveAudioChannel::UI:       S->UIVolume = Volume; break;
	case ERetrieveAudioChannel::Voice:    S->VoiceVolume = Volume; break;
	default: break;
	}
	if (bApplyNow)
	{
		ApplyAudio(S);
		OnSettingChanged.Broadcast(ERetrieveSettingsCategory::Audio);
	}
}

float URetrieveSettingsSubsystem::GetChannelVolume(ERetrieveAudioChannel Channel) const
{
	const URetrieveGameUserSettings* S = GetSettings();
	if (!S)
	{
		return 1.f;
	}
	switch (Channel)
	{
	case ERetrieveAudioChannel::Master:   return S->MasterVolume;
	case ERetrieveAudioChannel::Music:    return S->MusicVolume;
	case ERetrieveAudioChannel::Sfx:      return S->SfxVolume;
	case ERetrieveAudioChannel::Ambience: return S->AmbienceVolume;
	case ERetrieveAudioChannel::UI:       return S->UIVolume;
	case ERetrieveAudioChannel::Voice:    return S->VoiceVolume;
	default:                              return 1.f;
	}
}

void URetrieveSettingsSubsystem::SetColorBlind(ERetrieveColorBlindMode Mode, int32 Strength, bool bApplyNow)
{
	URetrieveGameUserSettings* S = GetSettings();
	if (!S)
	{
		return;
	}
	S->ColorBlindMode = Mode;
	S->ColorBlindStrength = FMath::Clamp(Strength, 0, 10);
	if (bApplyNow)
	{
		ApplyAccessibility(S);
		OnSettingChanged.Broadcast(ERetrieveSettingsCategory::Accessibility);
	}
}

bool URetrieveSettingsSubsystem::IsReduceMotionEnabled() const
{
	const URetrieveGameUserSettings* S = GetSettings();
	return S ? S->bReduceMotion : false;
}

// ── 적용 구현 ────────────────────────────────────────────────────

void URetrieveSettingsSubsystem::ApplyGraphics(URetrieveGameUserSettings* S, bool bApplyResolution)
{
	if (bApplyResolution)
	{
		S->ApplyResolutionSettings(/*bCheckForCommandLineOverrides*/ false);
	}
	S->ApplyNonResolutionSettings();

	if (GEngine)
	{
		GEngine->DisplayGamma = FMath::Clamp(S->GammaLevel, 1.0f, 4.0f);
	}
	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MotionBlurQuality")))
	{
		CVar->Set(S->bMotionBlur ? 4 : 0, ECVF_SetByGameSetting);
	}
}

void URetrieveSettingsSubsystem::ApplyAudio(URetrieveGameUserSettings* S)
{
	FApp::SetUnfocusedVolumeMultiplier(S->bMuteWhenUnfocused ? 0.f : 1.f);

	UWorld* World = GetSubsystemWorld();
	const URetrieveSettingsConfig* Cfg = GetDefault<URetrieveSettingsConfig>();
	if (!World || !Cfg)
	{
		return;
	}
	USoundMix* Mix = Cfg->SettingsSoundMix.LoadSynchronous();
	if (!Mix)
	{
		// 채널 라우팅 자산이 아직 없어도 마스터 볼륨은 실제 출력에 반영한다.
		FAudioDeviceHandle AudioDevice = World->GetAudioDevice();
		if (AudioDevice.IsValid())
		{
			AudioDevice->SetTransientPrimaryVolume(FMath::Clamp(S->MasterVolume, 0.f, 1.f));
		}
		return;
	}
	if (FAudioDeviceHandle AudioDevice = World->GetAudioDevice(); AudioDevice.IsValid())
	{
		AudioDevice->SetTransientPrimaryVolume(1.f);
	}

	auto ApplyChannel = [&](const TSoftObjectPtr<USoundClass>& SoftClass, float Volume)
	{
		if (USoundClass* SoundClass = SoftClass.LoadSynchronous())
		{
			UGameplayStatics::SetSoundMixClassOverride(World, Mix, SoundClass,
				FMath::Clamp(Volume, 0.f, 1.f), 1.0f, 0.2f, /*bApplyToChildren*/ true);
		}
	};

	ApplyChannel(Cfg->MasterSoundClass, S->MasterVolume);
	ApplyChannel(Cfg->MusicSoundClass, S->MusicVolume);
	ApplyChannel(Cfg->SfxSoundClass, S->SfxVolume);
	ApplyChannel(Cfg->AmbienceSoundClass, S->AmbienceVolume);
	ApplyChannel(Cfg->UISoundClass, S->UIVolume);
	ApplyChannel(Cfg->VoiceSoundClass, S->VoiceVolume);

	UGameplayStatics::PushSoundMixModifier(World, Mix);
}

void URetrieveSettingsSubsystem::ApplyControls(URetrieveGameUserSettings* S)
{
	if (const ULocalPlayer* LP = GetLocalPlayer())
	{
		if (APlayerController* PC = Cast<APlayerController>(LP->PlayerController))
		{
			PC->SetDisableHaptics(!S->bGamepadVibration);
		}
	}
	// 감도/반전/락온 모드는 게임플레이 코드가 GetSettings()로 직접 읽는다(브로드캐스트만).
}

void URetrieveSettingsSubsystem::ApplyGameplay(URetrieveGameUserSettings* S)
{
	if (!S->GameCulture.IsEmpty())
	{
		UKismetInternationalizationLibrary::SetCurrentCulture(S->GameCulture, /*SaveToConfig*/ true);
	}
	if (const ULocalPlayer* LP = GetLocalPlayer())
	{
		if (const APlayerController* PC = LP->PlayerController)
		{
			if (APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
			{
				CameraManager->SetFOV(FMath::Clamp(S->FieldOfView, 60.f, 120.f));
			}
		}
	}
	// 자막/튜토리얼 표시는 각 소비 UI가 GetSettings()로 참조한다.
}

void URetrieveSettingsSubsystem::ApplyAccessibility(URetrieveGameUserSettings* S)
{
	if (FSlateApplication::IsInitialized())
	{
		if (FSlateRenderer* Renderer = FSlateApplication::Get().GetRenderer())
		{
			EColorVisionDeficiency Type = EColorVisionDeficiency::NormalVision;
			switch (S->ColorBlindMode)
			{
			case ERetrieveColorBlindMode::Protanope:   Type = EColorVisionDeficiency::Protanope; break;
			case ERetrieveColorBlindMode::Deuteranope: Type = EColorVisionDeficiency::Deuteranope; break;
			case ERetrieveColorBlindMode::Tritanope:   Type = EColorVisionDeficiency::Tritanope; break;
			default:                                   Type = EColorVisionDeficiency::NormalVision; break;
			}
			const bool bCorrect = (S->ColorBlindMode != ERetrieveColorBlindMode::Off);
			Renderer->SetColorVisionDeficiencyType(Type, FMath::Clamp(S->ColorBlindStrength, 0, 10), bCorrect, false);
		}
	}
	// HighContrast / ReduceMotion / TextScale 은 위젯이 GetSettings()/IsReduceMotionEnabled()로 참조한다.
}
