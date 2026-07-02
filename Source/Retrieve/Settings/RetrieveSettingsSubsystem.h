#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Settings/RetrieveSettingsTypes.h"
#include "RetrieveSettingsSubsystem.generated.h"

class URetrieveGameUserSettings;
class APlayerController;
class APawn;

/** 카테고리 단위로 설정이 적용/변경됐음을 UI·게임플레이에 알린다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRetrieveOnSettingChanged, ERetrieveSettingsCategory, Category);

/**
 * 설정 적용 파사드(LocalPlayerSubsystem).
 * URetrieveGameUserSettings(저장)와 실제 런타임 적용(오디오 믹스·색맹·감마·컬처) 사이를 잇고,
 * 변경을 OnSettingChanged로 브로드캐스트한다. 설정 위젯/뷰모델은 이 서브시스템만 호출한다.
 */
UCLASS()
class RETRIEVE_API URetrieveSettingsSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintPure, Category = "Retrieve|Settings")
	static URetrieveSettingsSubsystem* Get(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "Retrieve|Settings")
	URetrieveGameUserSettings* GetSettings() const;

	/** 모든 카테고리를 적용하고 ini에 저장한다. (아래 생명주기 함수들의 편의 래퍼) */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Settings")
	void ApplyAllSettings(bool bSaveToDisk = true);

	// ── 적용 생명주기 ────────────────────────────────────────────
	// 각 설정은 필요한 객체가 준비된 시점에 적용되어야 한다.
	// 부팅 시점(LocalPlayerSubsystem::Initialize)에는 World/Controller/Pawn이 없으므로 전역만 적용하고,
	// 나머지는 PlayerController의 BeginPlay/AcknowledgePossession에서 호출된다.

	/** 그래픽/감마/모션블러/색맹/컬처 등 객체 의존이 없는 전역 설정. 설정 로드 직후 적용. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Settings")
	void ApplyGlobalSettings();

	/** 오디오 믹스 등 World/AudioDevice가 필요한 설정. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Settings")
	void ApplyWorldSettings(UWorld* World);

	/** 진동 등 PlayerController가 필요한 설정. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Settings")
	void ApplyControllerSettings(APlayerController* PlayerController);

	/** FOV 등 Pawn/Camera가 필요한 설정. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Settings")
	void ApplyPawnSettings(APawn* Pawn);

	/** UI 크기/고대비/모션 억제 등. 위젯이 구독해 즉시 갱신하도록 브로드캐스트만 한다. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Settings")
	void ApplyUISettings();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Settings")
	void ApplyCategory(ERetrieveSettingsCategory Category, bool bSaveToDisk = true);

	/** 카테고리를 기본값으로 되돌리고 적용한다. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Settings")
	void ResetCategory(ERetrieveSettingsCategory Category);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Settings")
	void SaveSettings();

	// ── 라이브 프리뷰용 세터(슬라이더 드래그 중 즉시 반영) ─────────
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Settings|Audio")
	void SetChannelVolume(ERetrieveAudioChannel Channel, float Volume, bool bApplyNow = true);

	UFUNCTION(BlueprintPure, Category = "Retrieve|Settings|Audio")
	float GetChannelVolume(ERetrieveAudioChannel Channel) const;

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Settings|Accessibility")
	void SetColorBlind(ERetrieveColorBlindMode Mode, int32 Strength, bool bApplyNow = true);

	/** 모션 억제 여부 질의(HUD VFX·카메라 흔들림에서 참조). */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Settings|Accessibility")
	bool IsReduceMotionEnabled() const;

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Settings")
	FRetrieveOnSettingChanged OnSettingChanged;

private:
	// ── 실제 적용 워커(명시적 컨텍스트) ──────────────────────────
	void ApplyGraphicsInternal(URetrieveGameUserSettings* S, bool bApplyResolution);
	void ApplyAudioInternal(URetrieveGameUserSettings* S, UWorld* World);
	void ApplyColorBlindInternal(URetrieveGameUserSettings* S);
	void ApplyCultureInternal(URetrieveGameUserSettings* S);
	void ApplyVibrationInternal(URetrieveGameUserSettings* S, APlayerController* PlayerController);
	void ApplyFOVInternal(URetrieveGameUserSettings* S, APawn* Pawn);
	void ApplyUIScaleInternal(const URetrieveGameUserSettings* S);

	UWorld* GetSubsystemWorld() const;
	APlayerController* GetSubsystemPlayerController() const;
	APawn* GetSubsystemPawn() const;
};
