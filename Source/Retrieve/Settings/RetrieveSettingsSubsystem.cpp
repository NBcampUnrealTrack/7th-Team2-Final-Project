#include "Settings/RetrieveSettingsSubsystem.h"
#include "Settings/RetrieveGameUserSettings.h"
#include "Settings/RetrieveSettingsConfig.h"

#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetInternationalizationLibrary.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundClass.h"
#include "Engine/UserInterfaceSettings.h"
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

	// 부팅 시점엔 World/Controller/Pawn이 아직 없다. 전역 설정만 즉시 적용하고,
	// 월드/컨트롤러/폰 종속 설정은 ARetrievePlayerController의 BeginPlay/AcknowledgePossession에서 적용된다.
	ApplyGlobalSettings();
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

APlayerController* URetrieveSettingsSubsystem::GetSubsystemPlayerController() const
{
	if (const ULocalPlayer* LP = GetLocalPlayer())
	{
		return Cast<APlayerController>(LP->PlayerController);
	}
	return nullptr;
}

APawn* URetrieveSettingsSubsystem::GetSubsystemPawn() const
{
	if (const APlayerController* PC = GetSubsystemPlayerController())
	{
		return PC->GetPawn();
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

	// 현재 사용 가능한 컨텍스트로 모든 생명주기 단계를 적용한다.
	ApplyGlobalSettings();
	ApplyWorldSettings(GetSubsystemWorld());
	ApplyControllerSettings(GetSubsystemPlayerController());
	ApplyPawnSettings(GetSubsystemPawn());
	ApplyUISettings();

	if (bSaveToDisk)
	{
		S->SaveSettings();
	}

	OnSettingChanged.Broadcast(ERetrieveSettingsCategory::MAX);
}

// ── 적용 생명주기 ────────────────────────────────────────────────

void URetrieveSettingsSubsystem::ApplyGlobalSettings()
{
	if (URetrieveGameUserSettings* S = GetSettings())
	{
		ApplyGraphicsInternal(S, /*bApplyResolution*/ true);
		ApplyColorBlindInternal(S);
		ApplyCultureInternal(S);
		ApplyUIScaleInternal(S); // 저장된 UI 크기를 부팅 시에도 적용
	}
}

void URetrieveSettingsSubsystem::ApplyWorldSettings(UWorld* World)
{
	if (URetrieveGameUserSettings* S = GetSettings())
	{
		// 사운드의 SoundClass는 DA_AudioRouting의 [Apply Routing To Assets] 버튼으로 에셋에 미리 구워둔다.
		// 따라서 런타임에는 로드/분류 없이 믹스만 적용한다.
		ApplyAudioInternal(S, World);
	}
}

void URetrieveSettingsSubsystem::ApplyControllerSettings(APlayerController* PlayerController)
{
	if (URetrieveGameUserSettings* S = GetSettings())
	{
		ApplyVibrationInternal(S, PlayerController);
	}
}

void URetrieveSettingsSubsystem::ApplyPawnSettings(APawn* Pawn)
{
	if (URetrieveGameUserSettings* S = GetSettings())
	{
		ApplyFOVInternal(S, Pawn);
	}
}

void URetrieveSettingsSubsystem::ApplyUISettings()
{
	// UI 크기는 부팅(ApplyGlobalSettings)과 Apply/Reset 확정 시점에만 적용한다.
	// (슬라이더 드래그 미리보기로 화면이 흔들리지 않도록 여기서는 적용하지 않는다.)
	// 고대비/모션 억제는 위젯이 RetrieveUISettingsLibrary로 읽으므로 갱신 신호만 보낸다.
	OnSettingChanged.Broadcast(ERetrieveSettingsCategory::Accessibility);
}

void URetrieveSettingsSubsystem::ApplyUIScaleInternal(const URetrieveGameUserSettings* S)
{
	const float Scale = FMath::Clamp(S->UITextScale, 0.5f, 2.f);

	// UMG 전용 DPI 스케일(게임 UMG에 직접 반영되는 주 경로).
	if (UUserInterfaceSettings* UISettings = GetMutableDefault<UUserInterfaceSettings>())
	{
		UISettings->ApplicationScale = Scale;
	}

	// 전역 Slate 스케일도 함께 적용(메뉴/HUD 일관). PIE에선 에디터 Slate도 스케일됨 → 확인은 Standalone 권장.
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().SetApplicationScale(Scale);
	}

	// 이미 떠 있는 위젯이 즉시 재배치되도록 레이아웃 무효화.
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().InvalidateAllWidgets(false);
	}
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
	case ERetrieveSettingsCategory::Graphics:
		// 저장(Apply 확정)일 때만 해상도까지 적용한다. 미리보기 중엔 창 크기 변동을 피한다.
		ApplyGraphicsInternal(S, /*bApplyResolution*/ bSaveToDisk);
		break;
	case ERetrieveSettingsCategory::Controls:
		ApplyVibrationInternal(S, GetSubsystemPlayerController());
		break;
	case ERetrieveSettingsCategory::Audio:
		ApplyAudioInternal(S, GetSubsystemWorld());
		break;
	case ERetrieveSettingsCategory::Gameplay:
		ApplyCultureInternal(S);
		ApplyFOVInternal(S, GetSubsystemPawn());
		break;
	case ERetrieveSettingsCategory::Accessibility:
		ApplyColorBlindInternal(S);
		if (bSaveToDisk)
		{
			// 확정(Apply/Reset)일 때만 UI 크기 적용. 미리보기(false)에선 적용하지 않는다.
			ApplyUIScaleInternal(S);
		}
		ApplyUISettings();
		break;
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

	// 해당 카테고리 값만 기본값으로 복원한다(다른 카테고리는 보존).
	S->ResetCategoryToDefaults(Category);

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
		ApplyAudioInternal(S, GetSubsystemWorld());
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
		ApplyColorBlindInternal(S);
		OnSettingChanged.Broadcast(ERetrieveSettingsCategory::Accessibility);
	}
}

bool URetrieveSettingsSubsystem::IsReduceMotionEnabled() const
{
	const URetrieveGameUserSettings* S = GetSettings();
	return S ? S->bReduceMotion : false;
}

// ── 적용 워커 구현 ───────────────────────────────────────────────

void URetrieveSettingsSubsystem::ApplyGraphicsInternal(URetrieveGameUserSettings* S, bool bApplyResolution)
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

void URetrieveSettingsSubsystem::ApplyAudioInternal(URetrieveGameUserSettings* S, UWorld* World)
{
	FApp::SetUnfocusedVolumeMultiplier(S->bMuteWhenUnfocused ? 0.f : 1.f);

	const URetrieveSettingsConfig* Cfg = GetDefault<URetrieveSettingsConfig>();
	if (!World || !Cfg)
	{
		return;
	}

	// 마스터는 항상 전역 볼륨(TransientPrimaryVolume)으로 적용한다.
	// SoundClass 미분류 사운드도 마스터를 따르므로, SoundMix가 없거나 사운드 재분류 전이어도 회귀하지 않는다.
	if (FAudioDeviceHandle AudioDevice = World->GetAudioDevice(); AudioDevice.IsValid())
	{
		AudioDevice->SetTransientPrimaryVolume(FMath::Clamp(S->MasterVolume, 0.f, 1.f));
	}

	USoundMix* Mix = Cfg->SettingsSoundMix.LoadSynchronous();
	if (!Mix)
	{
		// 경로가 지정됐는데 로드 실패한 경우만 경고(미지정은 정상 — 채널 믹스 미사용).
		if (!Cfg->SettingsSoundMix.IsNull())
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[RetrieveSettings] SettingsSoundMix 로드 실패: %s — 하위 채널(음악/효과/UI 등) 볼륨이 적용되지 않습니다."),
				*Cfg->SettingsSoundMix.ToString());
		}
		return;
	}

	// 하위 채널은 SoundMix의 SoundClass Override로 적용한다(해당 SoundClass에 분류된 사운드에만 반영).
	// 마스터는 위에서 전역 적용했으므로 SC_Master Override는 생략한다(이중 적용 방지).
	auto ApplyChannel = [&](const TSoftObjectPtr<USoundClass>& SoftClass, float Volume)
	{
		if (USoundClass* SoundClass = SoftClass.LoadSynchronous())
		{
			UGameplayStatics::SetSoundMixClassOverride(World, Mix, SoundClass,
				FMath::Clamp(Volume, 0.f, 1.f), 1.0f, 0.2f, /*bApplyToChildren*/ true);
		}
	};

	ApplyChannel(Cfg->MusicSoundClass, S->MusicVolume);
	ApplyChannel(Cfg->SfxSoundClass, S->SfxVolume);
	ApplyChannel(Cfg->AmbienceSoundClass, S->AmbienceVolume);
	ApplyChannel(Cfg->UISoundClass, S->UIVolume);
	ApplyChannel(Cfg->VoiceSoundClass, S->VoiceVolume);

	UGameplayStatics::PushSoundMixModifier(World, Mix);
}

void URetrieveSettingsSubsystem::ApplyVibrationInternal(URetrieveGameUserSettings* S, APlayerController* PlayerController)
{
	if (PlayerController)
	{
		PlayerController->SetDisableHaptics(!S->bGamepadVibration);
	}
	// 감도/반전/락온 모드는 게임플레이 코드가 GetSettings()로 직접 읽는다(브로드캐스트만).
}

void URetrieveSettingsSubsystem::ApplyCultureInternal(URetrieveGameUserSettings* S)
{
	if (!S->GameCulture.IsEmpty())
	{
		UKismetInternationalizationLibrary::SetCurrentCulture(S->GameCulture, /*SaveToConfig*/ true);
	}
	// 자막/튜토리얼 표시는 각 소비 UI가 GetSettings()로 참조한다.
}

void URetrieveSettingsSubsystem::ApplyFOVInternal(URetrieveGameUserSettings* S, APawn* Pawn)
{
	// 일반 플레이 카메라 FOV. Pawn(또는 그 컨트롤러)의 PlayerCameraManager에 적용한다.
	// (SovereignCharacter의 ThirdPersonCamera 직접 적용은 카메라 담당 협의 후 작업 3에서 다룬다.)
	APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : GetSubsystemPlayerController();
	if (PC && PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->SetFOV(FMath::Clamp(S->FieldOfView, 60.f, 120.f));
	}
}

void URetrieveSettingsSubsystem::ApplyColorBlindInternal(URetrieveGameUserSettings* S)
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
