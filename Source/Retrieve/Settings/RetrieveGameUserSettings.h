#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "Settings/RetrieveSettingsTypes.h"
#include "RetrieveGameUserSettings.generated.h"

/**
 * 프로젝트 사용자 설정 저장소.
 * 해상도 / 스케일러빌리티 / VSync / 프레임 제한은 UGameUserSettings 기본 기능을 사용하고,
 * 오디오·게임플레이·접근성·조작(비-리바인드) 값은 아래 config 필드로 확장한다.
 *
 * 값 변경 후 실제 적용/저장은 URetrieveSettingsSubsystem이 담당한다.
 * (DefaultEngine.ini의 GameUserSettingsClassName을 이 클래스로 지정해야 Get()이 동작한다.)
 */
UCLASS(BlueprintType)
class RETRIEVE_API URetrieveGameUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	URetrieveGameUserSettings();

	/** 현재 활성 인스턴스. GEngine 미초기화 시 nullptr. */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Settings")
	static URetrieveGameUserSettings* Get();

	virtual void SetToDefaults() override;

	/** 로드/저장 시 프로젝트 전용 값을 유효 범위로 보정한다(잘못된 ini 값 복구). */
	virtual void LoadSettings(bool bForceReload = false) override;
	virtual void SaveSettings() override;

	/**
	 * 한 카테고리의 값만 기본값으로 되돌린다(다른 카테고리는 보존).
	 * Graphics는 해상도/스케일러빌리티 등 엔진 그래픽 + Gamma/MotionBlur만 복원한다.
	 */
	void ResetCategoryToDefaults(ERetrieveSettingsCategory Category);

	/** 모든 프로젝트 전용 설정값을 유효 범위로 Clamp한다. */
	void ValidateRetrieveSettings();

	// ── Audio (0..1) ─────────────────────────────────────────────
	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Audio")
	float MasterVolume = 1.f;

	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Audio")
	float MusicVolume = 1.f;

	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Audio")
	float SfxVolume = 1.f;

	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Audio")
	float AmbienceVolume = 1.f;

	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Audio")
	float UIVolume = 1.f;

	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Audio")
	float VoiceVolume = 1.f;

	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Audio")
	bool bMuteWhenUnfocused = true;

	// ── Controls (비-리바인드) ───────────────────────────────────
	/** 감도 모드(배타). true=통합값만, false=X/Y만 적용. */
	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Controls")
	bool bUseUnifiedSensitivity = true;

	/** 통합 모드에서 쓰는 단일 마우스 감도(양축 공통). */
	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Controls")
	float MouseSensitivity = 1.f;

	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Controls")
	float MouseSensitivityX = 1.f;

	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Controls")
	float MouseSensitivityY = 1.f;

	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Controls")
	bool bInvertMouseY = false;

	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Controls")
	float GamepadSensitivityX = 1.f;

	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Controls")
	float GamepadSensitivityY = 1.f;

	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Controls")
	bool bGamepadVibration = true;

	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Controls")
	bool bLockOnToggleMode = true;

	/** true = 활 조준을 우클릭 1회로 켜고 다시 1회로 끄는 토글 방식. false(기본) = 우클릭을 누르고 있는 동안만 조준(홀드). */
	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Controls")
	bool bBowAimToggleMode = false;

	// ── Gameplay ─────────────────────────────────────────────────
	/** 빈 문자열이면 기본(현재) 컬처 유지. */
	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Gameplay")
	FString GameCulture;

	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Gameplay")
	bool bSubtitlesEnabled = true;

	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Gameplay")
	float SubtitleTextScale = 1.f;

	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Gameplay")
	float FieldOfView = 90.f;

	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Gameplay")
	float CameraShakeScale = 1.f;

	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Gameplay")
	bool bShowDamageNumbers = true;

	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Gameplay")
	bool bTutorialHints = true;

	// ── Accessibility ────────────────────────────────────────────
	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Accessibility")
	ERetrieveColorBlindMode ColorBlindMode = ERetrieveColorBlindMode::Off;

	/** 색맹 보정 강도 0..10. */
	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Accessibility")
	int32 ColorBlindStrength = 10;

	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Accessibility")
	float UITextScale = 1.f;

	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Accessibility")
	bool bHighContrastHUD = false;

	/** HUD VFX·카메라 흔들림 등 모션 연출을 억제. */
	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Accessibility")
	bool bReduceMotion = false;

	/** true = 상호작용을 길게 눌러 발동(홀드), false = 토글. */
	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Accessibility")
	bool bHoldToInteract = false;

	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Accessibility")
	float AimAssistStrength = 0.f;

	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Accessibility")
	float SubtitleBackgroundOpacity = 0.5f;

	// ── Graphics 확장 ────────────────────────────────────────────
	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Graphics")
	float GammaLevel = 2.2f;

	UPROPERTY(config, BlueprintReadWrite, Category = "Retrieve|Graphics")
	bool bMotionBlur = true;

	// ── 창 모드 BP 래퍼 ──────────────────────────────────────────
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Graphics")
	void SetRetrieveWindowMode(ERetrieveWindowMode Mode);

	UFUNCTION(BlueprintPure, Category = "Retrieve|Graphics")
	ERetrieveWindowMode GetRetrieveWindowMode() const;
};
