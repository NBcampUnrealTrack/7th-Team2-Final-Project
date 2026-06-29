#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Settings/RetrieveSettingsTypes.h"
#include "RetrieveSettingsSubsystem.generated.h"

class URetrieveGameUserSettings;

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

	/** 모든 카테고리를 적용하고 ini에 저장한다. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Settings")
	void ApplyAllSettings(bool bSaveToDisk = true);

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
	void ApplyGraphics(URetrieveGameUserSettings* S, bool bApplyResolution = true);
	void ApplyAudio(URetrieveGameUserSettings* S);
	void ApplyControls(URetrieveGameUserSettings* S);
	void ApplyGameplay(URetrieveGameUserSettings* S);
	void ApplyAccessibility(URetrieveGameUserSettings* S);

	UWorld* GetSubsystemWorld() const;
};
