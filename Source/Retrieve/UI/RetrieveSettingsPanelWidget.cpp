#include "UI/RetrieveSettingsPanelWidget.h"

#include "Settings/RetrieveGameUserSettings.h"
#include "Settings/RetrieveSettingsSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "PlayerMappableKeySettings.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "UObject/UnrealType.h"

namespace
{
	template <typename T>
	T* FindWidget(UUserWidget* Owner, const FName Name)
	{
		return Owner ? Cast<T>(Owner->GetWidgetFromName(Name)) : nullptr;
	}

	FText NumberText(const float Value, const int32 FractionalDigits = 0)
	{
		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = FractionalDigits;
		Options.MaximumFractionalDigits = FractionalDigits;
		return FText::AsNumber(Value, &Options);
	}

	FText PercentText(const float Value)
	{
		return NumberText(FMath::RoundToFloat(Value * 100.f));
	}

	void SetText(UUserWidget* Page, const FName Name, const FText& Text)
	{
		if (UTextBlock* TextBlock = FindWidget<UTextBlock>(Page, Name))
		{
			TextBlock->SetText(Text);
		}
	}

	void SetSlider(UUserWidget* Page, const FName Name, const float Value)
	{
		if (USlider* Slider = FindWidget<USlider>(Page, Name))
		{
			Slider->SetValue(Value);
		}
	}

	void SetChecked(UUserWidget* Page, const FName Name, const bool bChecked)
	{
		if (UCheckBox* CheckBox = FindWidget<UCheckBox>(Page, Name))
		{
			CheckBox->SetIsChecked(bChecked);
		}
	}

	int32 WrapIndex(const int32 Value, const int32 Count)
	{
		return (Value % Count + Count) % Count;
	}

	FText QualityText(const int32 Quality)
	{
		static const TCHAR* Labels[] = { TEXT("낮음"), TEXT("중간"), TEXT("높음"), TEXT("에픽") };
		return FText::FromString(Labels[FMath::Clamp(Quality, 0, 3)]);
	}

	FText WindowModeText(const ERetrieveWindowMode Mode)
	{
		switch (Mode)
		{
		case ERetrieveWindowMode::Fullscreen: return FText::FromString(TEXT("전체 화면"));
		case ERetrieveWindowMode::WindowedFullscreen: return FText::FromString(TEXT("테두리 없는 창"));
		default: return FText::FromString(TEXT("창 모드"));
		}
	}

	FText ColorBlindText(const ERetrieveColorBlindMode Mode)
	{
		switch (Mode)
		{
		case ERetrieveColorBlindMode::Protanope: return FText::FromString(TEXT("적색맹"));
		case ERetrieveColorBlindMode::Deuteranope: return FText::FromString(TEXT("녹색맹"));
		case ERetrieveColorBlindMode::Tritanope: return FText::FromString(TEXT("청색맹"));
		default: return FText::FromString(TEXT("끄기"));
		}
	}

	void EnsurePlayerMappableAction(const TCHAR* ActionPath, const FName MappingName, const FText& DisplayName)
	{
		UInputAction* Action = LoadObject<UInputAction>(nullptr, ActionPath);
		if (!Action || Action->GetPlayerMappableKeySettings())
		{
			return;
		}

		UPlayerMappableKeySettings* MappingSettings =
			NewObject<UPlayerMappableKeySettings>(Action, NAME_None, RF_Transient);
		MappingSettings->Name = MappingName;
		MappingSettings->DisplayName = DisplayName;
		MappingSettings->DisplayCategory = FText::FromString(TEXT("전투"));

		if (FObjectProperty* Property = FindFProperty<FObjectProperty>(
			UInputAction::StaticClass(), TEXT("PlayerMappableKeySettings")))
		{
			Property->SetObjectPropertyValue_InContainer(Action, MappingSettings);
		}
	}
}

void URetrieveSettingsPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindScreenEvents();
	BindPageEvents();
	ApplyRuntimeStyle();
	SelectCategory(ERetrieveSettingsCategory::Graphics);
}

FReply URetrieveSettingsPanelWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (!PendingMappingName.IsNone())
	{
		FinishRebindWithKey(InKeyEvent.GetKey());
		return FReply::Handled();
	}

	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		RequestClose();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply URetrieveSettingsPanelWidget::NativeOnPreviewMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	// 리바인드 대기 중일 때 마우스 버튼을 선점하여 키로 등록한다.
	if (!PendingMappingName.IsNone())
	{
		const FKey PressedKey = InMouseEvent.GetEffectingButton();
		if (PressedKey.IsValid())
		{
			FinishRebindWithKey(PressedKey);
			return FReply::Handled();
		}
	}
	return Super::NativeOnPreviewMouseButtonDown(InGeometry, InMouseEvent);
}

FName URetrieveSettingsPanelWidget::GetConflictingAction(const FKey& Key) const
{
	UEnhancedInputUserSettings* InputSettings = GetEnhancedInputUserSettings();
	const UEnhancedPlayerMappableKeyProfile* Profile = InputSettings ? InputSettings->GetActiveKeyProfile() : nullptr;
	if (!Profile) return NAME_None;

	for (const TPair<FName, FKeyMappingRow>& Pair : Profile->GetPlayerMappingRows())
	{
		if (Pair.Key == PendingMappingName) continue;
		for (const FPlayerKeyMapping& Mapping : Pair.Value.Mappings)
		{
			if (Mapping.GetCurrentKey() == Key && !Mapping.GetCurrentKey().IsGamepadKey())
			{
				const UInputAction* Action = Mapping.GetAssociatedInputAction();
				return Action ? Action->GetFName() : Pair.Key;
			}
		}
	}
	return NAME_None;
}

static FString ActionToKorean(const FName& ActionName)
{
	const FString S = ActionName.ToString();
	if (S == TEXT("IA_Attack"))  return TEXT("공격");
	if (S == TEXT("IA_Roll"))    return TEXT("회피");
	if (S == TEXT("IA_LockOn"))  return TEXT("록온");
	if (S.StartsWith(TEXT("IA_"))) return S.RightChop(3);
	return S;
}

void URetrieveSettingsPanelWidget::FinishRebindWithKey(const FKey& NewKey)
{
	if (NewKey == EKeys::Escape)
	{
		PendingMappingName = NAME_None;
		PendingKeyLabelName = NAME_None;
		RefreshKeyBindings();
		return;
	}

	if (!NewKey.IsValid() || NewKey.IsGamepadKey())
	{
		return;
	}

	// 중복 키 검사
	const FName ConflictAction = GetConflictingAction(NewKey);
	if (!ConflictAction.IsNone())
	{
		const FString Warning = FString::Printf(TEXT("⚠ 이미 사용 중: %s"), *ActionToKorean(ConflictAction));
		SetText(GetPage(ERetrieveSettingsCategory::Controls), PendingKeyLabelName, FText::FromString(Warning));

		// 이전 타이머 초기화 후 2초 뒤 복원
		GetWorld()->GetTimerManager().ClearTimer(RebindWarningTimerHandle);
		GetWorld()->GetTimerManager().SetTimer(RebindWarningTimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				RefreshKeyBindings();
			}),
			2.0f, false);

		PendingMappingName = NAME_None;
		PendingKeyLabelName = NAME_None;
		return;
	}

	if (UEnhancedInputUserSettings* InputSettings = GetEnhancedInputUserSettings())
	{
		FMapPlayerKeyArgs Args;
		Args.MappingName = PendingMappingName;
		Args.Slot = EPlayerMappableKeySlot::First;
		Args.NewKey = NewKey;
		Args.bCreateMatchingSlotIfNeeded = true;
		FGameplayTagContainer FailureReason;
		InputSettings->MapPlayerKey(Args, FailureReason);
		if (FailureReason.IsEmpty())
		{
			InputSettings->ApplySettings();
			InputSettings->AsyncSaveSettings();
			SetText(GetPage(ERetrieveSettingsCategory::Controls), PendingKeyLabelName, NewKey.GetDisplayName());
		}
	}
	PendingMappingName = NAME_None;
	PendingKeyLabelName = NAME_None;
}

URetrieveSettingsSubsystem* URetrieveSettingsPanelWidget::GetSettingsSubsystem() const
{
	return URetrieveSettingsSubsystem::Get(this);
}

URetrieveGameUserSettings* URetrieveSettingsPanelWidget::GetUserSettings() const
{
	return URetrieveGameUserSettings::Get();
}

void URetrieveSettingsPanelWidget::SelectCategory(const ERetrieveSettingsCategory Category)
{
	if (Category >= ERetrieveSettingsCategory::MAX)
	{
		return;
	}

	CurrentCategory = Category;
	UpdateScreenForCategory(Category);
	OnCategorySelected(Category);
	OnTabSwitchRequested.Broadcast(static_cast<int32>(Category));
}

void URetrieveSettingsPanelWidget::ApplyAndSave()
{
	if (URetrieveSettingsSubsystem* Subsystem = GetSettingsSubsystem())
	{
		Subsystem->ApplyAllSettings(true);
	}
	OnSettingsApplied();
}

void URetrieveSettingsPanelWidget::ResetCurrentCategory()
{
	if (URetrieveSettingsSubsystem* Subsystem = GetSettingsSubsystem())
	{
		Subsystem->ResetCategory(CurrentCategory);
	}
	RefreshCurrentPage();
}

UUserWidget* URetrieveSettingsPanelWidget::GetPage(const ERetrieveSettingsCategory Category) const
{
	static const FName PageNames[] = {
		TEXT("Page_Graphics"), TEXT("Page_Controls"), TEXT("Page_Audio"),
		TEXT("Page_Gameplay"), TEXT("Page_Accessibility")
	};
	const int32 Index = static_cast<int32>(Category);
	return Index >= 0 && Index < UE_ARRAY_COUNT(PageNames)
		? FindWidget<UUserWidget>(const_cast<URetrieveSettingsPanelWidget*>(this), PageNames[Index])
		: nullptr;
}

void URetrieveSettingsPanelWidget::BindScreenEvents()
{
#define BIND_SCREEN_BUTTON(Name, Handler) \
	if (UButton* Button = FindWidget<UButton>(this, TEXT(Name))) \
	{ Button->OnClicked.AddUniqueDynamic(this, &ThisClass::Handler); RegisterSoundButton(Button); }
	BIND_SCREEN_BUTTON("Btn_Cat_Graphics", HandleGraphicsTab)
	BIND_SCREEN_BUTTON("Btn_Cat_Controls", HandleControlsTab)
	BIND_SCREEN_BUTTON("Btn_Cat_Audio", HandleAudioTab)
	BIND_SCREEN_BUTTON("Btn_Cat_Gameplay", HandleGameplayTab)
	BIND_SCREEN_BUTTON("Btn_Cat_Accessibility", HandleAccessibilityTab)
	BIND_SCREEN_BUTTON("Btn_Apply", HandleApply)
	BIND_SCREEN_BUTTON("Btn_Reset", HandleReset)
	BIND_SCREEN_BUTTON("Btn_Close", HandleClose)
#undef BIND_SCREEN_BUTTON
}

void URetrieveSettingsPanelWidget::BindPageEvents()
{
#define BIND_PAGE_BUTTON(Category, Name, Handler) \
	if (UButton* Button = FindWidget<UButton>(GetPage(Category), TEXT(Name))) \
	{ Button->OnClicked.AddUniqueDynamic(this, &ThisClass::Handler); RegisterSoundButton(Button); }
#define BIND_PAGE_SLIDER(Category, Name, Handler) \
	if (USlider* Slider = FindWidget<USlider>(GetPage(Category), TEXT(Name))) \
	{ Slider->OnValueChanged.AddUniqueDynamic(this, &ThisClass::Handler); }
#define BIND_PAGE_CHECK(Category, Name, Handler) \
	if (UCheckBox* CheckBox = FindWidget<UCheckBox>(GetPage(Category), TEXT(Name))) \
	{ CheckBox->OnCheckStateChanged.AddUniqueDynamic(this, &ThisClass::Handler); }

	using C = ERetrieveSettingsCategory;
	BIND_PAGE_BUTTON(C::Graphics, "Btn_WindowMode_Prev", HandleWindowModePrev)
	BIND_PAGE_BUTTON(C::Graphics, "Btn_WindowMode_Next", HandleWindowModeNext)
	BIND_PAGE_BUTTON(C::Graphics, "Btn_Resolution_Prev", HandleResolutionPrev)
	BIND_PAGE_BUTTON(C::Graphics, "Btn_Resolution_Next", HandleResolutionNext)
	BIND_PAGE_BUTTON(C::Graphics, "Btn_Quality_Prev", HandleQualityPrev)
	BIND_PAGE_BUTTON(C::Graphics, "Btn_Quality_Next", HandleQualityNext)
	BIND_PAGE_BUTTON(C::Graphics, "Btn_Shadow_Prev", HandleShadowPrev)
	BIND_PAGE_BUTTON(C::Graphics, "Btn_Shadow_Next", HandleShadowNext)
	BIND_PAGE_BUTTON(C::Graphics, "Btn_Texture_Prev", HandleTexturePrev)
	BIND_PAGE_BUTTON(C::Graphics, "Btn_Texture_Next", HandleTextureNext)
	BIND_PAGE_BUTTON(C::Graphics, "Btn_Effects_Prev", HandleEffectsPrev)
	BIND_PAGE_BUTTON(C::Graphics, "Btn_Effects_Next", HandleEffectsNext)
	BIND_PAGE_CHECK(C::Graphics, "Chk_VSync", HandleVSyncChanged)
	BIND_PAGE_CHECK(C::Graphics, "Chk_MotionBlur", HandleMotionBlurChanged)
	BIND_PAGE_SLIDER(C::Graphics, "Sld_FrameLimit", HandleFrameLimitChanged)
	BIND_PAGE_SLIDER(C::Graphics, "Sld_Gamma", HandleGammaChanged)

	BIND_PAGE_SLIDER(C::Controls, "Sld_MouseX", HandleMouseXChanged)
	BIND_PAGE_SLIDER(C::Controls, "Sld_MouseY", HandleMouseYChanged)
	BIND_PAGE_SLIDER(C::Controls, "Sld_PadSens", HandlePadSensitivityChanged)
	BIND_PAGE_CHECK(C::Controls, "Chk_InvertY", HandleInvertYChanged)
	BIND_PAGE_CHECK(C::Controls, "Chk_Vibration", HandleVibrationChanged)
	BIND_PAGE_BUTTON(C::Controls, "Btn_LockOn_Prev", HandleLockOnPrev)
	BIND_PAGE_BUTTON(C::Controls, "Btn_LockOn_Next", HandleLockOnNext)
	BIND_PAGE_BUTTON(C::Controls, "KeyBtn_Attack", HandleRebindAttack)
	BIND_PAGE_BUTTON(C::Controls, "KeyBtn_Dodge", HandleRebindDodge)
	BIND_PAGE_BUTTON(C::Controls, "KeyBtn_LockOn", HandleRebindLockOn)

	BIND_PAGE_SLIDER(C::Audio, "Sld_Master", HandleMasterChanged)
	BIND_PAGE_SLIDER(C::Audio, "Sld_Music", HandleMusicChanged)
	BIND_PAGE_SLIDER(C::Audio, "Sld_Sfx", HandleSfxChanged)
	BIND_PAGE_SLIDER(C::Audio, "Sld_Ambience", HandleAmbienceChanged)
	BIND_PAGE_SLIDER(C::Audio, "Sld_UI", HandleUIChanged)
	BIND_PAGE_SLIDER(C::Audio, "Sld_Voice", HandleVoiceChanged)
	BIND_PAGE_CHECK(C::Audio, "Chk_MuteUnfocused", HandleMuteUnfocusedChanged)

	BIND_PAGE_BUTTON(C::Gameplay, "Btn_Language_Prev", HandleLanguagePrev)
	BIND_PAGE_BUTTON(C::Gameplay, "Btn_Language_Next", HandleLanguageNext)
	BIND_PAGE_CHECK(C::Gameplay, "Chk_Subtitles", HandleSubtitlesChanged)
	BIND_PAGE_CHECK(C::Gameplay, "Chk_DamageNumbers", HandleDamageNumbersChanged)
	BIND_PAGE_CHECK(C::Gameplay, "Chk_TutorialHints", HandleTutorialHintsChanged)
	BIND_PAGE_SLIDER(C::Gameplay, "Sld_SubtitleScale", HandleSubtitleScaleChanged)
	BIND_PAGE_SLIDER(C::Gameplay, "Sld_FOV", HandleFOVChanged)
	BIND_PAGE_SLIDER(C::Gameplay, "Sld_CameraShake", HandleCameraShakeChanged)

	BIND_PAGE_BUTTON(C::Accessibility, "Btn_ColorBlind_Prev", HandleColorBlindPrev)
	BIND_PAGE_BUTTON(C::Accessibility, "Btn_ColorBlind_Next", HandleColorBlindNext)
	BIND_PAGE_BUTTON(C::Accessibility, "Btn_Interact_Prev", HandleInteractPrev)
	BIND_PAGE_BUTTON(C::Accessibility, "Btn_Interact_Next", HandleInteractNext)
	BIND_PAGE_SLIDER(C::Accessibility, "Sld_CBStrength", HandleColorBlindStrengthChanged)
	BIND_PAGE_SLIDER(C::Accessibility, "Sld_UIScale", HandleUIScaleChanged)
	BIND_PAGE_SLIDER(C::Accessibility, "Sld_AimAssist", HandleAimAssistChanged)
	BIND_PAGE_SLIDER(C::Accessibility, "Sld_SubtitleBG", HandleSubtitleBackgroundChanged)
	BIND_PAGE_CHECK(C::Accessibility, "Chk_HighContrast", HandleHighContrastChanged)
	BIND_PAGE_CHECK(C::Accessibility, "Chk_ReduceMotion", HandleReduceMotionChanged)

#undef BIND_PAGE_CHECK
#undef BIND_PAGE_SLIDER
#undef BIND_PAGE_BUTTON
}

void URetrieveSettingsPanelWidget::UpdateScreenForCategory(const ERetrieveSettingsCategory Category)
{
	if (UWidgetSwitcher* Switcher = FindWidget<UWidgetSwitcher>(this, TEXT("Switcher_Pages")))
	{
		Switcher->SetActiveWidgetIndex(static_cast<int32>(Category));
	}

	const FLinearColor DarkChip(0.12f, 0.10f, 0.07f, 0.55f);
	const FLinearColor Gold(0.80f, 0.62f, 0.28f, 0.90f);
	static const FName ButtonNames[] = {
		TEXT("Btn_Cat_Graphics"), TEXT("Btn_Cat_Controls"), TEXT("Btn_Cat_Audio"),
		TEXT("Btn_Cat_Gameplay"), TEXT("Btn_Cat_Accessibility")
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ButtonNames); ++Index)
	{
		if (UButton* Button = FindWidget<UButton>(this, ButtonNames[Index]))
		{
			Button->SetBackgroundColor(Index == static_cast<int32>(Category) ? Gold : DarkChip);
		}
	}
	RefreshPage(Category);
}

void URetrieveSettingsPanelWidget::RefreshCurrentPage()
{
	RefreshPage(CurrentCategory);
}

void URetrieveSettingsPanelWidget::RefreshPage(const ERetrieveSettingsCategory Category)
{
	TGuardValue<bool> RefreshGuard(bRefreshingControls, true);
	switch (Category)
	{
	case ERetrieveSettingsCategory::Graphics: RefreshGraphics(); break;
	case ERetrieveSettingsCategory::Controls: RefreshControls(); break;
	case ERetrieveSettingsCategory::Audio: RefreshAudio(); break;
	case ERetrieveSettingsCategory::Gameplay: RefreshGameplay(); break;
	case ERetrieveSettingsCategory::Accessibility: RefreshAccessibility(); break;
	default: break;
	}
}

void URetrieveSettingsPanelWidget::RefreshGraphics()
{
	URetrieveGameUserSettings* S = GetUserSettings();
	UUserWidget* Page = GetPage(ERetrieveSettingsCategory::Graphics);
	if (!S || !Page) return;
	SetText(Page, TEXT("Val_WindowMode"), WindowModeText(S->GetRetrieveWindowMode()));
	{
		const FIntPoint Res = S->GetScreenResolution();
		SetText(Page, TEXT("Val_Resolution"), FText::FromString(FString::Printf(TEXT("%d x %d"), Res.X, Res.Y)));
	}
	SetText(Page, TEXT("Val_Quality"), QualityText(S->GetOverallScalabilityLevel()));
	SetText(Page, TEXT("Val_Shadow"), QualityText(S->GetShadowQuality()));
	SetText(Page, TEXT("Val_Texture"), QualityText(S->GetTextureQuality()));
	SetText(Page, TEXT("Val_Effects"), QualityText(S->GetVisualEffectQuality()));
	SetChecked(Page, TEXT("Chk_VSync"), S->IsVSyncEnabled());
	SetChecked(Page, TEXT("Chk_MotionBlur"), S->bMotionBlur);
	SetSlider(Page, TEXT("Sld_FrameLimit"), S->GetFrameRateLimit());
	SetSlider(Page, TEXT("Sld_Gamma"), S->GammaLevel);
	SetText(Page, TEXT("Val_FrameLimit"), NumberText(S->GetFrameRateLimit()));
	SetText(Page, TEXT("Val_Gamma"), NumberText(S->GammaLevel, 1));
}

void URetrieveSettingsPanelWidget::RefreshControls()
{
	URetrieveGameUserSettings* S = GetUserSettings();
	UUserWidget* Page = GetPage(ERetrieveSettingsCategory::Controls);
	if (!S || !Page) return;
	SetSlider(Page, TEXT("Sld_MouseX"), S->MouseSensitivityX);
	SetSlider(Page, TEXT("Sld_MouseY"), S->MouseSensitivityY);
	SetSlider(Page, TEXT("Sld_PadSens"), S->GamepadSensitivityX);
	SetText(Page, TEXT("Val_MouseX"), NumberText(S->MouseSensitivityX, 1));
	SetText(Page, TEXT("Val_MouseY"), NumberText(S->MouseSensitivityY, 1));
	SetText(Page, TEXT("Val_PadSens"), NumberText(S->GamepadSensitivityX, 1));
	SetChecked(Page, TEXT("Chk_InvertY"), S->bInvertMouseY);
	SetChecked(Page, TEXT("Chk_Vibration"), S->bGamepadVibration);
	SetText(Page, TEXT("Val_LockOn"), FText::FromString(S->bLockOnToggleMode ? TEXT("토글") : TEXT("홀드")));
	RefreshKeyBindings();
}

UEnhancedInputUserSettings* URetrieveSettingsPanelWidget::GetEnhancedInputUserSettings() const
{
	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	if (!LocalPlayer)
	{
		return nullptr;
	}

	UEnhancedInputUserSettings* InputSettings = nullptr;
	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
	{
		InputSettings = InputSubsystem->GetUserSettings();
	}
	if (!InputSettings)
	{
		InputSettings = UEnhancedInputUserSettings::LoadOrCreateSettings(LocalPlayer);
	}
	if (InputSettings)
	{
		EnsurePlayerMappableAction(
			TEXT("/Game/Retrieve/Input/Actions/IA_Attack.IA_Attack"), TEXT("Retrieve.Attack"), FText::FromString(TEXT("공격")));
		EnsurePlayerMappableAction(
			TEXT("/Game/Retrieve/Input/Actions/IA_Roll.IA_Roll"), TEXT("Retrieve.Dodge"), FText::FromString(TEXT("회피")));
		EnsurePlayerMappableAction(
			TEXT("/Game/Retrieve/Input/Actions/IA_LockOn.IA_LockOn"), TEXT("Retrieve.LockOn"), FText::FromString(TEXT("락온")));
		if (const UInputMappingContext* Context = LoadObject<UInputMappingContext>(
			nullptr, TEXT("/Game/Retrieve/Input/IMC_Default.IMC_Default")))
		{
			InputSettings->RegisterInputMappingContext(Context);
		}
	}
	return InputSettings;
}

void URetrieveSettingsPanelWidget::RefreshKeyBindings()
{
	UEnhancedInputUserSettings* InputSettings = GetEnhancedInputUserSettings();
	UUserWidget* Page = GetPage(ERetrieveSettingsCategory::Controls);
	if (!InputSettings || !Page)
	{
		return;
	}

	struct FKeyRow { FName Action; FName Label; };
	static const FKeyRow Rows[] = {
		{TEXT("IA_Attack"), TEXT("Key_Attack")},
		{TEXT("IA_Roll"), TEXT("Key_Dodge")},
		{TEXT("IA_LockOn"), TEXT("Key_LockOn")}
	};
	const UEnhancedPlayerMappableKeyProfile* Profile = InputSettings->GetActiveKeyProfile();
	if (!Profile)
	{
		return;
	}
	for (const FKeyRow& Row : Rows)
	{
		FText Display = FText::FromString(TEXT("미지정"));
		for (const TPair<FName, FKeyMappingRow>& Pair : Profile->GetPlayerMappingRows())
		{
			for (const FPlayerKeyMapping& Mapping : Pair.Value.Mappings)
			{
				const UInputAction* Action = Mapping.GetAssociatedInputAction();
				if (Action && Action->GetFName() == Row.Action && !Mapping.GetCurrentKey().IsGamepadKey())
				{
					Display = Mapping.GetCurrentKey().GetDisplayName();
					break;
				}
			}
		}
		SetText(Page, Row.Label, Display);
	}
}

void URetrieveSettingsPanelWidget::BeginRebind(const FName ActionAssetName, const FName LabelWidgetName)
{
	UEnhancedInputUserSettings* InputSettings = GetEnhancedInputUserSettings();
	const UEnhancedPlayerMappableKeyProfile* Profile = InputSettings ? InputSettings->GetActiveKeyProfile() : nullptr;
	if (!Profile)
	{
		return;
	}

	// 이전 경고 타이머가 돌고 있으면 취소
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(RebindWarningTimerHandle);

	PendingMappingName = NAME_None;
	for (const TPair<FName, FKeyMappingRow>& Pair : Profile->GetPlayerMappingRows())
	{
		for (const FPlayerKeyMapping& Mapping : Pair.Value.Mappings)
		{
			const UInputAction* Action = Mapping.GetAssociatedInputAction();
			if (Action && Action->GetFName() == ActionAssetName && !Mapping.GetCurrentKey().IsGamepadKey())
			{
				PendingMappingName = Pair.Key;
				break;
			}
		}
		if (!PendingMappingName.IsNone()) break;
	}

	PendingKeyLabelName = LabelWidgetName;
	SetText(GetPage(ERetrieveSettingsCategory::Controls), LabelWidgetName,
		FText::FromString(PendingMappingName.IsNone() ? TEXT("매핑 설정 필요") : TEXT("키를 누르세요")));

	// 다음 키 입력을 받으려면 이 위젯에 포커스가 있어야 한다.
	SetKeyboardFocus();
}

void URetrieveSettingsPanelWidget::RefreshAudio()
{
	URetrieveSettingsSubsystem* Subsystem = GetSettingsSubsystem();
	URetrieveGameUserSettings* S = GetUserSettings();
	UUserWidget* Page = GetPage(ERetrieveSettingsCategory::Audio);
	if (!Subsystem || !S || !Page) return;
	struct FChannelRow { ERetrieveAudioChannel Channel; FName Slider; FName Value; };
	static const FChannelRow Rows[] = {
		{ERetrieveAudioChannel::Master, TEXT("Sld_Master"), TEXT("Val_Master")},
		{ERetrieveAudioChannel::Music, TEXT("Sld_Music"), TEXT("Val_Music")},
		{ERetrieveAudioChannel::Sfx, TEXT("Sld_Sfx"), TEXT("Val_Sfx")},
		{ERetrieveAudioChannel::Ambience, TEXT("Sld_Ambience"), TEXT("Val_Ambience")},
		{ERetrieveAudioChannel::UI, TEXT("Sld_UI"), TEXT("Val_UI")},
		{ERetrieveAudioChannel::Voice, TEXT("Sld_Voice"), TEXT("Val_Voice")}
	};
	for (const FChannelRow& Row : Rows)
	{
		const float Value = Subsystem->GetChannelVolume(Row.Channel);
		SetSlider(Page, Row.Slider, Value);
		SetText(Page, Row.Value, PercentText(Value));
	}
	SetChecked(Page, TEXT("Chk_MuteUnfocused"), S->bMuteWhenUnfocused);
}

void URetrieveSettingsPanelWidget::RefreshGameplay()
{
	URetrieveGameUserSettings* S = GetUserSettings();
	UUserWidget* Page = GetPage(ERetrieveSettingsCategory::Gameplay);
	if (!S || !Page) return;
	SetText(Page, TEXT("Val_Language"), FText::FromString(S->GameCulture == TEXT("en") ? TEXT("English") : TEXT("한국어")));
	SetChecked(Page, TEXT("Chk_Subtitles"), S->bSubtitlesEnabled);
	SetChecked(Page, TEXT("Chk_DamageNumbers"), S->bShowDamageNumbers);
	SetChecked(Page, TEXT("Chk_TutorialHints"), S->bTutorialHints);
	SetSlider(Page, TEXT("Sld_SubtitleScale"), S->SubtitleTextScale);
	SetSlider(Page, TEXT("Sld_FOV"), S->FieldOfView);
	SetSlider(Page, TEXT("Sld_CameraShake"), S->CameraShakeScale);
	SetText(Page, TEXT("Val_SubtitleScale"), PercentText(S->SubtitleTextScale));
	SetText(Page, TEXT("Val_FOV"), NumberText(S->FieldOfView));
	SetText(Page, TEXT("Val_CameraShake"), PercentText(S->CameraShakeScale));
}

void URetrieveSettingsPanelWidget::RefreshAccessibility()
{
	URetrieveGameUserSettings* S = GetUserSettings();
	UUserWidget* Page = GetPage(ERetrieveSettingsCategory::Accessibility);
	if (!S || !Page) return;
	SetText(Page, TEXT("Val_ColorBlind"), ColorBlindText(S->ColorBlindMode));
	SetText(Page, TEXT("Val_Interact"), FText::FromString(S->bHoldToInteract ? TEXT("홀드") : TEXT("토글")));
	SetSlider(Page, TEXT("Sld_CBStrength"), S->ColorBlindStrength);
	SetSlider(Page, TEXT("Sld_UIScale"), S->UITextScale);
	SetSlider(Page, TEXT("Sld_AimAssist"), S->AimAssistStrength);
	SetSlider(Page, TEXT("Sld_SubtitleBG"), S->SubtitleBackgroundOpacity);
	SetText(Page, TEXT("Val_CBStrength"), NumberText(S->ColorBlindStrength));
	SetText(Page, TEXT("Val_UIScale"), PercentText(S->UITextScale));
	SetText(Page, TEXT("Val_AimAssist"), PercentText(S->AimAssistStrength));
	SetText(Page, TEXT("Val_SubtitleBG"), PercentText(S->SubtitleBackgroundOpacity));
	SetChecked(Page, TEXT("Chk_HighContrast"), S->bHighContrastHUD);
	SetChecked(Page, TEXT("Chk_ReduceMotion"), S->bReduceMotion);
}

void URetrieveSettingsPanelWidget::ApplyRuntimeStyle()
{
	const FLinearColor BarColor(0.30f, 0.24f, 0.12f, 1.f);
	const FLinearColor HandleColor(0.90f, 0.74f, 0.38f, 1.f);
	const FLinearColor DarkChip(0.12f, 0.10f, 0.07f, 0.55f);
	for (int32 CategoryIndex = 0; CategoryIndex < static_cast<int32>(ERetrieveSettingsCategory::MAX); ++CategoryIndex)
	{
		UUserWidget* Page = GetPage(static_cast<ERetrieveSettingsCategory>(CategoryIndex));
		if (!Page) continue;
		static const FName SliderNames[] = {
			TEXT("Sld_FrameLimit"), TEXT("Sld_Gamma"), TEXT("Sld_MouseX"), TEXT("Sld_MouseY"), TEXT("Sld_PadSens"),
			TEXT("Sld_Master"), TEXT("Sld_Music"), TEXT("Sld_Sfx"), TEXT("Sld_Ambience"), TEXT("Sld_UI"), TEXT("Sld_Voice"),
			TEXT("Sld_SubtitleScale"), TEXT("Sld_FOV"), TEXT("Sld_CameraShake"), TEXT("Sld_CBStrength"),
			TEXT("Sld_UIScale"), TEXT("Sld_AimAssist"), TEXT("Sld_SubtitleBG")
		};
		for (const FName Name : SliderNames)
		{
			if (USlider* Slider = FindWidget<USlider>(Page, Name))
			{
				Slider->SetSliderBarColor(BarColor);
				Slider->SetSliderHandleColor(HandleColor);
			}
		}
		static const FName ButtonNames[] = {
			TEXT("Btn_WindowMode_Prev"), TEXT("Btn_WindowMode_Next"), TEXT("Btn_Quality_Prev"), TEXT("Btn_Quality_Next"),
			TEXT("Btn_Shadow_Prev"), TEXT("Btn_Shadow_Next"), TEXT("Btn_Texture_Prev"), TEXT("Btn_Texture_Next"),
			TEXT("Btn_Effects_Prev"), TEXT("Btn_Effects_Next"), TEXT("Btn_LockOn_Prev"), TEXT("Btn_LockOn_Next"),
			TEXT("Btn_Language_Prev"), TEXT("Btn_Language_Next"), TEXT("Btn_ColorBlind_Prev"), TEXT("Btn_ColorBlind_Next"),
			TEXT("Btn_Interact_Prev"), TEXT("Btn_Interact_Next"), TEXT("Btn_Attack"), TEXT("Btn_Dodge"), TEXT("Btn_LockOn")
		};
		for (const FName Name : ButtonNames)
		{
			if (UButton* Button = FindWidget<UButton>(Page, Name)) Button->SetBackgroundColor(DarkChip);
		}
	}

	auto SetRange = [this](const ERetrieveSettingsCategory Category, const FName Name, const float Min, const float Max)
	{
		if (USlider* Slider = FindWidget<USlider>(GetPage(Category), Name))
		{
			Slider->SetMinValue(Min);
			Slider->SetMaxValue(Max);
		}
	};
	SetRange(ERetrieveSettingsCategory::Graphics, TEXT("Sld_FrameLimit"), 30.f, 240.f);
	SetRange(ERetrieveSettingsCategory::Graphics, TEXT("Sld_Gamma"), 1.8f, 2.6f);
	SetRange(ERetrieveSettingsCategory::Controls, TEXT("Sld_MouseX"), 0.1f, 3.f);
	SetRange(ERetrieveSettingsCategory::Controls, TEXT("Sld_MouseY"), 0.1f, 3.f);
	SetRange(ERetrieveSettingsCategory::Controls, TEXT("Sld_PadSens"), 0.1f, 3.f);
	SetRange(ERetrieveSettingsCategory::Gameplay, TEXT("Sld_SubtitleScale"), 0.5f, 2.f);
	SetRange(ERetrieveSettingsCategory::Gameplay, TEXT("Sld_FOV"), 70.f, 110.f);
	SetRange(ERetrieveSettingsCategory::Gameplay, TEXT("Sld_CameraShake"), 0.f, 1.f);
	SetRange(ERetrieveSettingsCategory::Accessibility, TEXT("Sld_CBStrength"), 0.f, 10.f);
	SetRange(ERetrieveSettingsCategory::Accessibility, TEXT("Sld_UIScale"), 0.8f, 1.5f);
}

void URetrieveSettingsPanelWidget::HandleGraphicsTab() { SelectCategory(ERetrieveSettingsCategory::Graphics); }
void URetrieveSettingsPanelWidget::HandleControlsTab() { SelectCategory(ERetrieveSettingsCategory::Controls); }
void URetrieveSettingsPanelWidget::HandleAudioTab() { SelectCategory(ERetrieveSettingsCategory::Audio); }
void URetrieveSettingsPanelWidget::HandleGameplayTab() { SelectCategory(ERetrieveSettingsCategory::Gameplay); }
void URetrieveSettingsPanelWidget::HandleAccessibilityTab() { SelectCategory(ERetrieveSettingsCategory::Accessibility); }
void URetrieveSettingsPanelWidget::HandleApply() { ApplyAndSave(); }
void URetrieveSettingsPanelWidget::HandleReset() { ResetCurrentCategory(); }
void URetrieveSettingsPanelWidget::HandleClose() { RequestClose(); }

#define APPLY_PREVIEW(Category) if (URetrieveSettingsSubsystem* Subsystem = GetSettingsSubsystem()) Subsystem->ApplyCategory(Category, false)
#define DEFINE_QUALITY_HANDLER(Func, Getter, Setter, Delta, ValueName) \
	void URetrieveSettingsPanelWidget::Func() { if (URetrieveGameUserSettings* S = GetUserSettings()) { \
	const int32 V = WrapIndex(S->Getter() + Delta, 4); S->Setter(V); \
	APPLY_PREVIEW(ERetrieveSettingsCategory::Graphics); RefreshCurrentPage(); } }

void URetrieveSettingsPanelWidget::HandleWindowModePrev() { if (URetrieveGameUserSettings* S = GetUserSettings()) { const auto V = static_cast<ERetrieveWindowMode>(WrapIndex(static_cast<int32>(S->GetRetrieveWindowMode()) - 1, 3)); S->SetRetrieveWindowMode(V); SetText(GetPage(ERetrieveSettingsCategory::Graphics), TEXT("Val_WindowMode"), WindowModeText(V)); APPLY_PREVIEW(ERetrieveSettingsCategory::Graphics); } }
void URetrieveSettingsPanelWidget::HandleWindowModeNext() { if (URetrieveGameUserSettings* S = GetUserSettings()) { const auto V = static_cast<ERetrieveWindowMode>(WrapIndex(static_cast<int32>(S->GetRetrieveWindowMode()) + 1, 3)); S->SetRetrieveWindowMode(V); SetText(GetPage(ERetrieveSettingsCategory::Graphics), TEXT("Val_WindowMode"), WindowModeText(V)); APPLY_PREVIEW(ERetrieveSettingsCategory::Graphics); } }

namespace
{
	static const FIntPoint SupportedResolutions[] = {
		{1280, 720}, {1920, 1080}, {2560, 1440}, {3840, 2160}
	};
	static const int32 ResolutionCount = UE_ARRAY_COUNT(SupportedResolutions);

	int32 FindCurrentResolutionIndex(const FIntPoint& Current)
	{
		for (int32 i = 0; i < ResolutionCount; ++i)
		{
			if (SupportedResolutions[i] == Current) return i;
		}
		return 1; // 기본값: 1920x1080
	}
}

void URetrieveSettingsPanelWidget::HandleResolutionPrev()
{
	if (URetrieveGameUserSettings* S = GetUserSettings())
	{
		const int32 Idx = WrapIndex(FindCurrentResolutionIndex(S->GetScreenResolution()) - 1, ResolutionCount);
		const FIntPoint Next = SupportedResolutions[Idx];
		S->SetScreenResolution(Next);
		// Apply는 하지 않는다 — 즉시 적용 시 창 크기가 바뀌어 UI가 잘린다. Apply 버튼 클릭 시 적용된다.
		SetText(GetPage(ERetrieveSettingsCategory::Graphics), TEXT("Val_Resolution"),
			FText::FromString(FString::Printf(TEXT("%d x %d"), Next.X, Next.Y)));
	}
}

void URetrieveSettingsPanelWidget::HandleResolutionNext()
{
	if (URetrieveGameUserSettings* S = GetUserSettings())
	{
		const int32 Idx = WrapIndex(FindCurrentResolutionIndex(S->GetScreenResolution()) + 1, ResolutionCount);
		const FIntPoint Next = SupportedResolutions[Idx];
		S->SetScreenResolution(Next);
		// Apply는 하지 않는다 — 즉시 적용 시 창 크기가 바뀌어 UI가 잘린다. Apply 버튼 클릭 시 적용된다.
		SetText(GetPage(ERetrieveSettingsCategory::Graphics), TEXT("Val_Resolution"),
			FText::FromString(FString::Printf(TEXT("%d x %d"), Next.X, Next.Y)));
	}
}
DEFINE_QUALITY_HANDLER(HandleQualityPrev, GetOverallScalabilityLevel, SetOverallScalabilityLevel, -1, "Val_Quality")
DEFINE_QUALITY_HANDLER(HandleQualityNext, GetOverallScalabilityLevel, SetOverallScalabilityLevel, 1, "Val_Quality")
DEFINE_QUALITY_HANDLER(HandleShadowPrev, GetShadowQuality, SetShadowQuality, -1, "Val_Shadow")
DEFINE_QUALITY_HANDLER(HandleShadowNext, GetShadowQuality, SetShadowQuality, 1, "Val_Shadow")
DEFINE_QUALITY_HANDLER(HandleTexturePrev, GetTextureQuality, SetTextureQuality, -1, "Val_Texture")
DEFINE_QUALITY_HANDLER(HandleTextureNext, GetTextureQuality, SetTextureQuality, 1, "Val_Texture")
DEFINE_QUALITY_HANDLER(HandleEffectsPrev, GetVisualEffectQuality, SetVisualEffectQuality, -1, "Val_Effects")
DEFINE_QUALITY_HANDLER(HandleEffectsNext, GetVisualEffectQuality, SetVisualEffectQuality, 1, "Val_Effects")
#undef DEFINE_QUALITY_HANDLER

void URetrieveSettingsPanelWidget::HandleVSyncChanged(bool b) { if (bRefreshingControls) return; if (auto* S = GetUserSettings()) { S->SetVSyncEnabled(b); APPLY_PREVIEW(ERetrieveSettingsCategory::Graphics); } }
void URetrieveSettingsPanelWidget::HandleMotionBlurChanged(bool b) { if (bRefreshingControls) return; if (auto* S = GetUserSettings()) { S->bMotionBlur = b; APPLY_PREVIEW(ERetrieveSettingsCategory::Graphics); } }
void URetrieveSettingsPanelWidget::HandleFrameLimitChanged(float V) { if (bRefreshingControls) return; if (auto* S = GetUserSettings()) { S->SetFrameRateLimit(V); SetText(GetPage(ERetrieveSettingsCategory::Graphics), TEXT("Val_FrameLimit"), NumberText(V)); } }
void URetrieveSettingsPanelWidget::HandleGammaChanged(float V) { if (bRefreshingControls) return; if (auto* S = GetUserSettings()) { S->GammaLevel = V; SetText(GetPage(ERetrieveSettingsCategory::Graphics), TEXT("Val_Gamma"), NumberText(V, 1)); APPLY_PREVIEW(ERetrieveSettingsCategory::Graphics); } }

#define DEFINE_FLOAT_SETTING_HANDLER(Func, Field, Category, PageCategory, Label, Digits) \
	void URetrieveSettingsPanelWidget::Func(float V) { if (bRefreshingControls) return; if (auto* S = GetUserSettings()) { S->Field = V; \
	SetText(GetPage(PageCategory), TEXT(Label), NumberText(V, Digits)); APPLY_PREVIEW(Category); } }
DEFINE_FLOAT_SETTING_HANDLER(HandleMouseXChanged, MouseSensitivityX, ERetrieveSettingsCategory::Controls, ERetrieveSettingsCategory::Controls, "Val_MouseX", 1)
DEFINE_FLOAT_SETTING_HANDLER(HandleMouseYChanged, MouseSensitivityY, ERetrieveSettingsCategory::Controls, ERetrieveSettingsCategory::Controls, "Val_MouseY", 1)
DEFINE_FLOAT_SETTING_HANDLER(HandlePadSensitivityChanged, GamepadSensitivityX, ERetrieveSettingsCategory::Controls, ERetrieveSettingsCategory::Controls, "Val_PadSens", 1)
void URetrieveSettingsPanelWidget::HandleInvertYChanged(bool b) { if (bRefreshingControls) return; if (auto* S = GetUserSettings()) { S->bInvertMouseY = b; APPLY_PREVIEW(ERetrieveSettingsCategory::Controls); } }
void URetrieveSettingsPanelWidget::HandleVibrationChanged(bool b) { if (bRefreshingControls) return; if (auto* S = GetUserSettings()) { S->bGamepadVibration = b; APPLY_PREVIEW(ERetrieveSettingsCategory::Controls); } }
void URetrieveSettingsPanelWidget::HandleLockOnPrev() { HandleLockOnNext(); }
void URetrieveSettingsPanelWidget::HandleLockOnNext() { if (auto* S = GetUserSettings()) { S->bLockOnToggleMode = !S->bLockOnToggleMode; SetText(GetPage(ERetrieveSettingsCategory::Controls), TEXT("Val_LockOn"), FText::FromString(S->bLockOnToggleMode ? TEXT("토글") : TEXT("홀드"))); APPLY_PREVIEW(ERetrieveSettingsCategory::Controls); } }
void URetrieveSettingsPanelWidget::HandleRebindAttack() { BeginRebind(TEXT("IA_Attack"), TEXT("Key_Attack")); }
void URetrieveSettingsPanelWidget::HandleRebindDodge() { BeginRebind(TEXT("IA_Roll"), TEXT("Key_Dodge")); }
void URetrieveSettingsPanelWidget::HandleRebindLockOn() { BeginRebind(TEXT("IA_LockOn"), TEXT("Key_LockOn")); }

#define DEFINE_AUDIO_HANDLER(Func, Channel, Label) \
	void URetrieveSettingsPanelWidget::Func(float V) { if (bRefreshingControls) return; if (auto* Subsystem = GetSettingsSubsystem()) { \
	Subsystem->SetChannelVolume(Channel, V, true); SetText(GetPage(ERetrieveSettingsCategory::Audio), TEXT(Label), PercentText(V)); } }
DEFINE_AUDIO_HANDLER(HandleMasterChanged, ERetrieveAudioChannel::Master, "Val_Master")
DEFINE_AUDIO_HANDLER(HandleMusicChanged, ERetrieveAudioChannel::Music, "Val_Music")
DEFINE_AUDIO_HANDLER(HandleSfxChanged, ERetrieveAudioChannel::Sfx, "Val_Sfx")
DEFINE_AUDIO_HANDLER(HandleAmbienceChanged, ERetrieveAudioChannel::Ambience, "Val_Ambience")
DEFINE_AUDIO_HANDLER(HandleUIChanged, ERetrieveAudioChannel::UI, "Val_UI")
DEFINE_AUDIO_HANDLER(HandleVoiceChanged, ERetrieveAudioChannel::Voice, "Val_Voice")
#undef DEFINE_AUDIO_HANDLER
void URetrieveSettingsPanelWidget::HandleMuteUnfocusedChanged(bool b) { if (bRefreshingControls) return; if (auto* S = GetUserSettings()) S->bMuteWhenUnfocused = b; }

void URetrieveSettingsPanelWidget::HandleLanguagePrev() { HandleLanguageNext(); }
void URetrieveSettingsPanelWidget::HandleLanguageNext() { if (auto* S = GetUserSettings()) { S->GameCulture = S->GameCulture == TEXT("en") ? TEXT("ko") : TEXT("en"); SetText(GetPage(ERetrieveSettingsCategory::Gameplay), TEXT("Val_Language"), FText::FromString(S->GameCulture == TEXT("en") ? TEXT("English") : TEXT("한국어"))); APPLY_PREVIEW(ERetrieveSettingsCategory::Gameplay); } }
void URetrieveSettingsPanelWidget::HandleSubtitlesChanged(bool b) { if (bRefreshingControls) return; if (auto* S = GetUserSettings()) { S->bSubtitlesEnabled = b; APPLY_PREVIEW(ERetrieveSettingsCategory::Gameplay); } }
void URetrieveSettingsPanelWidget::HandleDamageNumbersChanged(bool b) { if (bRefreshingControls) return; if (auto* S = GetUserSettings()) { S->bShowDamageNumbers = b; APPLY_PREVIEW(ERetrieveSettingsCategory::Gameplay); } }
void URetrieveSettingsPanelWidget::HandleTutorialHintsChanged(bool b) { if (bRefreshingControls) return; if (auto* S = GetUserSettings()) { S->bTutorialHints = b; APPLY_PREVIEW(ERetrieveSettingsCategory::Gameplay); } }
void URetrieveSettingsPanelWidget::HandleSubtitleScaleChanged(float V) { if (bRefreshingControls) return; if (auto* S = GetUserSettings()) { S->SubtitleTextScale = V; SetText(GetPage(ERetrieveSettingsCategory::Gameplay), TEXT("Val_SubtitleScale"), PercentText(V)); APPLY_PREVIEW(ERetrieveSettingsCategory::Gameplay); } }
DEFINE_FLOAT_SETTING_HANDLER(HandleFOVChanged, FieldOfView, ERetrieveSettingsCategory::Gameplay, ERetrieveSettingsCategory::Gameplay, "Val_FOV", 0)
void URetrieveSettingsPanelWidget::HandleCameraShakeChanged(float V) { if (bRefreshingControls) return; if (auto* S = GetUserSettings()) { S->CameraShakeScale = V; SetText(GetPage(ERetrieveSettingsCategory::Gameplay), TEXT("Val_CameraShake"), PercentText(V)); APPLY_PREVIEW(ERetrieveSettingsCategory::Gameplay); } }

void URetrieveSettingsPanelWidget::HandleColorBlindPrev() { if (auto* S = GetUserSettings()) { const auto M = static_cast<ERetrieveColorBlindMode>(WrapIndex(static_cast<int32>(S->ColorBlindMode) - 1, 4)); if (auto* Subsystem = GetSettingsSubsystem()) Subsystem->SetColorBlind(M, S->ColorBlindStrength, true); SetText(GetPage(ERetrieveSettingsCategory::Accessibility), TEXT("Val_ColorBlind"), ColorBlindText(M)); } }
void URetrieveSettingsPanelWidget::HandleColorBlindNext() { if (auto* S = GetUserSettings()) { const auto M = static_cast<ERetrieveColorBlindMode>(WrapIndex(static_cast<int32>(S->ColorBlindMode) + 1, 4)); if (auto* Subsystem = GetSettingsSubsystem()) Subsystem->SetColorBlind(M, S->ColorBlindStrength, true); SetText(GetPage(ERetrieveSettingsCategory::Accessibility), TEXT("Val_ColorBlind"), ColorBlindText(M)); } }
void URetrieveSettingsPanelWidget::HandleInteractPrev() { HandleInteractNext(); }
void URetrieveSettingsPanelWidget::HandleInteractNext() { if (auto* S = GetUserSettings()) { S->bHoldToInteract = !S->bHoldToInteract; SetText(GetPage(ERetrieveSettingsCategory::Accessibility), TEXT("Val_Interact"), FText::FromString(S->bHoldToInteract ? TEXT("홀드") : TEXT("토글"))); APPLY_PREVIEW(ERetrieveSettingsCategory::Accessibility); } }
void URetrieveSettingsPanelWidget::HandleColorBlindStrengthChanged(float V) { if (bRefreshingControls) return; if (auto* S = GetUserSettings()) { if (auto* Subsystem = GetSettingsSubsystem()) Subsystem->SetColorBlind(S->ColorBlindMode, FMath::RoundToInt(V), true); SetText(GetPage(ERetrieveSettingsCategory::Accessibility), TEXT("Val_CBStrength"), NumberText(FMath::RoundToInt(V))); } }
void URetrieveSettingsPanelWidget::HandleUIScaleChanged(float V) { if (bRefreshingControls) return; if (auto* S = GetUserSettings()) { S->UITextScale = V; SetText(GetPage(ERetrieveSettingsCategory::Accessibility), TEXT("Val_UIScale"), PercentText(V)); APPLY_PREVIEW(ERetrieveSettingsCategory::Accessibility); } }
void URetrieveSettingsPanelWidget::HandleAimAssistChanged(float V) { if (bRefreshingControls) return; if (auto* S = GetUserSettings()) { S->AimAssistStrength = V; SetText(GetPage(ERetrieveSettingsCategory::Accessibility), TEXT("Val_AimAssist"), PercentText(V)); APPLY_PREVIEW(ERetrieveSettingsCategory::Accessibility); } }
void URetrieveSettingsPanelWidget::HandleSubtitleBackgroundChanged(float V) { if (bRefreshingControls) return; if (auto* S = GetUserSettings()) { S->SubtitleBackgroundOpacity = V; SetText(GetPage(ERetrieveSettingsCategory::Accessibility), TEXT("Val_SubtitleBG"), PercentText(V)); APPLY_PREVIEW(ERetrieveSettingsCategory::Accessibility); } }
void URetrieveSettingsPanelWidget::HandleHighContrastChanged(bool b) { if (bRefreshingControls) return; if (auto* S = GetUserSettings()) { S->bHighContrastHUD = b; APPLY_PREVIEW(ERetrieveSettingsCategory::Accessibility); } }
void URetrieveSettingsPanelWidget::HandleReduceMotionChanged(bool b) { if (bRefreshingControls) return; if (auto* S = GetUserSettings()) { S->bReduceMotion = b; APPLY_PREVIEW(ERetrieveSettingsCategory::Accessibility); } }

#undef DEFINE_FLOAT_SETTING_HANDLER
#undef APPLY_PREVIEW
