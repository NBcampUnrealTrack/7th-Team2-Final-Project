#pragma once

#include "CoreMinimal.h"
#include "UI/RetrieveGamePanelWidget.h"
#include "Settings/RetrieveSettingsTypes.h"
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

protected:
	virtual void NativeConstruct() override;
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
	UUserWidget* GetPage(ERetrieveSettingsCategory Category) const;
	UEnhancedInputUserSettings* GetEnhancedInputUserSettings() const;
	void RefreshKeyBindings();
	void BeginRebind(FName ActionAssetName, FName LabelWidgetName);

	bool bRefreshingControls = false;
	FName PendingMappingName;
	FName PendingKeyLabelName;
	FTimerHandle RebindWarningTimerHandle;

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
