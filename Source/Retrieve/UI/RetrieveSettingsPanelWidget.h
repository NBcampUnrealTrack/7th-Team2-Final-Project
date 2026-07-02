#pragma once

#include "CoreMinimal.h"
#include "UI/RetrieveGamePanelWidget.h"
#include "Settings/RetrieveSettingsTypes.h"
#include "Settings/RetrieveSettingsSnapshot.h"
#include "RetrieveSettingsPanelWidget.generated.h"

class URetrieveSettingsSubsystem;
class URetrieveGameUserSettings;
class UUserWidget;
class UEnhancedInputUserSettings;

/**
 * 설정 화면(WBP_SettingsScreen)의 C++ 베이스.
 * URetrieveGamePanelWidget의 열기/닫기 VFX·ToggleKey·RequestClose를 그대로 사용하면서,
 * 카테고리 전환과 Apply/Reset/Save를 서브시스템으로 위임한다.
 */
UCLASS(Abstract, Blueprintable)
class RETRIEVE_API URetrieveSettingsPanelWidget : public URetrieveGamePanelWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Settings")
	URetrieveSettingsSubsystem* GetSettingsSubsystem() const;

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Retrieve|Settings")
	URetrieveGameUserSettings* GetUserSettings() const;

	/** 좌측 레일에서 카테고리를 선택. 베이스의 OnTabSwitchRequested도 함께 브로드캐스트한다. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Settings")
	void SelectCategory(ERetrieveSettingsCategory Category);

	UFUNCTION(BlueprintPure, Category = "Retrieve|Settings")
	ERetrieveSettingsCategory GetCurrentCategory() const { return CurrentCategory; }

	/** Apply 버튼: 전체 적용 + 저장 후 BP에 알림. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Settings")
	void ApplyAndSave();

	/** 현재 카테고리를 기본값으로 되돌린다. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Settings")
	void ResetCurrentCategory();

	/** 카테고리 전환 시 BP가 WidgetSwitcher 인덱스를 갱신하도록 호출된다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|Settings")
	void OnCategorySelected(ERetrieveSettingsCategory Category);

	/** Apply 완료 후 BP 후처리(토스트/사운드 등). */
	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|Settings")
	void OnSettingsApplied();

	// ── 해상도/창 모드 확인 트랜잭션 ─────────────────────────────
	/** Apply로 해상도·창 모드가 바뀌면 호출된다. BP가 확인 팝업(WBP_SettingsConfirmResolution 등)을 띄운다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|Settings")
	void OnResolutionConfirmRequested(float TimeoutSeconds);

	/** 확인/원복으로 트랜잭션이 끝났을 때 BP가 팝업을 닫도록 호출된다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|Settings")
	void OnResolutionConfirmResolved();

	/** 사용자가 "유지"를 선택. 새 해상도를 확정한다. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Settings")
	void ConfirmResolution();

	/** 사용자가 "되돌리기"를 선택하거나 시간 초과. 이전 해상도/창 모드로 복구한다. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Settings")
	void RevertResolution();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** 해상도/창 모드 확인 기능 사용 여부. 확인 팝업 WBP가 준비되기 전엔 false로 두면 변경이 즉시 확정된다. */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Settings")
	bool bEnableResolutionConfirm = true;

	/** 해상도 확인 대기 시간(초). 이 시간 내 확인이 없으면 자동 원복. */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Settings", meta = (ClampMin = "5.0", ClampMax = "30.0"))
	float ResolutionConfirmTimeoutSeconds = 12.f;

	/**
	 * 해상도 확인 팝업 WBP(WBP_SettingsConfirmResolution).
	 * 지정되면 Apply로 해상도가 바뀔 때 이 팝업을 띄우고 Btn_Keep/Btn_Revert를
	 * ConfirmResolution/RevertResolution에 자동 배선한다. 미지정이면 OnResolutionConfirmRequested(BP)로 위임.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Settings")
	TSubclassOf<UUserWidget> ResolutionConfirmPopupClass;

	/** 현재 떠 있는 확인 팝업 인스턴스. */
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> ActiveResolutionPopup;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Settings")
	ERetrieveSettingsCategory CurrentCategory = ERetrieveSettingsCategory::Graphics;

private:
	void BindScreenEvents();
	void BindPageEvents();
	void RefreshCurrentPage();
	void RefreshPage(ERetrieveSettingsCategory Category);
	void RefreshGraphics();
	void RefreshControls();
	void RefreshAudio();
	void RefreshGameplay();
	void RefreshAccessibility();
	void UpdateScreenForCategory(ERetrieveSettingsCategory Category);
	void ApplyRuntimeStyle();
	/** 소비 구현이 없는 옵션 행을 숨긴다(RetrieveSettingAvailability 플래그 기반). */
	void ApplyOptionAvailability();
	UUserWidget* GetPage(ERetrieveSettingsCategory Category) const;
	UEnhancedInputUserSettings* GetEnhancedInputUserSettings() const;
	void RefreshKeyBindings();
	void BeginRebind(FName ActionAssetName, FName LabelWidgetName);

	bool bRefreshingControls = false;
	FName PendingMappingName;
	FName PendingKeyLabelName;
	FTimerHandle RebindWarningTimerHandle;

	// ── Pending/Apply/Cancel 트랜잭션 ───────────────────────────
	/** 화면을 열 때의 확정값. Apply 없이 닫으면 이 값으로 원복한다. Apply 시 현재값으로 갱신. */
	FRetrieveSettingsSnapshot BaselineSnapshot;
	/** 해상도 확인 트랜잭션 중 직전(원복용) 표시 모드 스냅샷. */
	FRetrieveSettingsSnapshot PreConfirmSnapshot;
	bool bAwaitingResolutionConfirm = false;
	FTimerHandle ResolutionConfirmTimerHandle;

	// 확인 팝업 카운트다운 표시용.
	FTimerHandle ResolutionCountdownTimerHandle;
	double ResolutionConfirmEndTime = 0.0;

	/** BaselineSnapshot을 설정에 되돌리고 런타임을 재적용한다(저장하지 않음). */
	void RevertToBaseline();

	/** 확인 팝업을 생성·표시하고 Btn_Keep/Btn_Revert를 배선한다(ResolutionConfirmPopupClass 지정 시). */
	void ShowResolutionConfirmPopup(float TimeoutSeconds);
	/** 확인 팝업을 닫고 카운트다운 타이머를 정리한다. */
	void HideResolutionConfirmPopup();
	/** 팝업의 Txt_Countdown에 남은 시간을 갱신한다. */
	void UpdateResolutionCountdown();

	/** 리바인드 확정 처리 (중복 검사 + EnhancedInput 적용). 키보드/마우스 양쪽에서 호출된다. */
	void FinishRebindWithKey(const FKey& NewKey);
	/** NewKey 를 이미 쓰고 있는 다른 매핑 이름을 반환. 없으면 NAME_None. */
	FName GetConflictingAction(const FKey& Key) const;

	UFUNCTION() void HandleGraphicsTab();
	UFUNCTION() void HandleControlsTab();
	UFUNCTION() void HandleAudioTab();
	UFUNCTION() void HandleGameplayTab();
	UFUNCTION() void HandleAccessibilityTab();
	UFUNCTION() void HandleApply();
	UFUNCTION() void HandleReset();
	UFUNCTION() void HandleClose();

	/** 설정 변경(특히 접근성/고대비) 시 화면 스타일을 즉시 다시 적용한다. */
	UFUNCTION() void HandleSettingChanged(ERetrieveSettingsCategory Category);

	/** Audio 페이지의 슬라이더/토글 행 아키타입을 찾아 값 변경을 구독한다. */
	void BindAudioRows();

	/** 슬라이더 행 값(0..1) 변경 시 RowKey를 채널로 매핑해 볼륨을 적용한다. */
	UFUNCTION() void HandleAudioRowChanged(FName RowKey, float Value);
	/** 토글 행 On/Off 변경 시 처리. */
	UFUNCTION() void HandleAudioToggleChanged(FName RowKey, bool bValue);

	/** Graphics/Controls/Gameplay 페이지의 WBP_SettingRow_Slider/Toggle 행을 찾아 라벨 설정 및 값 변경 구독. */
	void BindGraphicsRows();
	void BindControlsRows();
	void BindGameplayRows();
	void BindAccessibilityRows();

	UFUNCTION() void HandleGraphicsRowChanged(FName RowKey, float Value);
	UFUNCTION() void HandleGraphicsToggleChanged(FName RowKey, bool bValue);
	UFUNCTION() void HandleControlsRowChanged(FName RowKey, float Value);
	UFUNCTION() void HandleControlsToggleChanged(FName RowKey, bool bValue);
	UFUNCTION() void HandleGameplayRowChanged(FName RowKey, float Value);
	UFUNCTION() void HandleGameplayToggleChanged(FName RowKey, bool bValue);
	UFUNCTION() void HandleAccessibilityRowChanged(FName RowKey, float Value);
	UFUNCTION() void HandleAccessibilityToggleChanged(FName RowKey, bool bValue);

	UFUNCTION() void HandleWindowModePrev();
	UFUNCTION() void HandleWindowModeNext();
	UFUNCTION() void HandleResolutionPrev();
	UFUNCTION() void HandleResolutionNext();
	UFUNCTION() void HandleQualityPrev();
	UFUNCTION() void HandleQualityNext();
	UFUNCTION() void HandleShadowPrev();
	UFUNCTION() void HandleShadowNext();
	UFUNCTION() void HandleTexturePrev();
	UFUNCTION() void HandleTextureNext();
	UFUNCTION() void HandleEffectsPrev();
	UFUNCTION() void HandleEffectsNext();
	UFUNCTION() void HandleVSyncChanged(bool bChecked);
	UFUNCTION() void HandleMotionBlurChanged(bool bChecked);
	UFUNCTION() void HandleFrameLimitChanged(float Value);
	UFUNCTION() void HandleGammaChanged(float Value);

	UFUNCTION() void HandleMouseXChanged(float Value);
	UFUNCTION() void HandleMouseYChanged(float Value);
	UFUNCTION() void HandlePadSensitivityChanged(float Value);
	UFUNCTION() void HandleInvertYChanged(bool bChecked);
	UFUNCTION() void HandleVibrationChanged(bool bChecked);
	UFUNCTION() void HandleLockOnPrev();
	UFUNCTION() void HandleLockOnNext();
	UFUNCTION() void HandleRebindAttack();
	UFUNCTION() void HandleRebindDodge();
	UFUNCTION() void HandleRebindLockOn();

	UFUNCTION() void HandleMasterChanged(float Value);
	UFUNCTION() void HandleMusicChanged(float Value);
	UFUNCTION() void HandleSfxChanged(float Value);
	UFUNCTION() void HandleAmbienceChanged(float Value);
	UFUNCTION() void HandleUIChanged(float Value);
	UFUNCTION() void HandleVoiceChanged(float Value);
	UFUNCTION() void HandleMuteUnfocusedChanged(bool bChecked);

	UFUNCTION() void HandleLanguagePrev();
	UFUNCTION() void HandleLanguageNext();
	UFUNCTION() void HandleSubtitlesChanged(bool bChecked);
	UFUNCTION() void HandleDamageNumbersChanged(bool bChecked);
	UFUNCTION() void HandleTutorialHintsChanged(bool bChecked);
	UFUNCTION() void HandleSubtitleScaleChanged(float Value);
	UFUNCTION() void HandleFOVChanged(float Value);
	UFUNCTION() void HandleCameraShakeChanged(float Value);

	UFUNCTION() void HandleColorBlindPrev();
	UFUNCTION() void HandleColorBlindNext();
	UFUNCTION() void HandleInteractPrev();
	UFUNCTION() void HandleInteractNext();
	UFUNCTION() void HandleColorBlindStrengthChanged(float Value);
	UFUNCTION() void HandleUIScaleChanged(float Value);
	UFUNCTION() void HandleAimAssistChanged(float Value);
	UFUNCTION() void HandleSubtitleBackgroundChanged(float Value);
	UFUNCTION() void HandleHighContrastChanged(bool bChecked);
	UFUNCTION() void HandleReduceMotionChanged(bool bChecked);
};
