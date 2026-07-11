#include "UI/RetrieveSettingsPanelWidget.h"

#include "Core/RetrieveGameState.h"
#include "Player/RetrievePlayerController.h"
#include "Settings/RetrieveGameUserSettings.h"
#include "Settings/RetrieveSettingsSubsystem.h"
#include "Settings/RetrieveSettingAvailability.h"
#include "UI/RetrieveUISettingsLibrary.h"
#include "UI/RetrieveUITheme.h"
#include "UI/Settings/RetrieveSettingRowSlider.h"
#include "UI/Settings/RetrieveSettingRowToggle.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "Kismet/KismetSystemLibrary.h"
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

	// URetrieveSettingRowToggle 위젯을 RowKey로 찾아 상태를 조용히 설정
	void SetToggleByKey(UUserWidget* Page, const FName RowKey, const bool bValue)
	{
		if (!Page || !Page->WidgetTree) return;
		Page->WidgetTree->ForEachWidget([RowKey, bValue](UWidget* W)
		{
			if (URetrieveSettingRowToggle* Toggle = Cast<URetrieveSettingRowToggle>(W))
			{
				if (Toggle->RowKey == RowKey)
				{
					Toggle->SetValueSilently(bValue);
				}
			}
		});
	}

	int32 WrapIndex(const int32 Value, const int32 Count)
	{
		return (Value % Count + Count) % Count;
	}

	FText QualityText(const int32 Quality)
	{
		// GetOverallScalabilityLevel은 하위 품질이 서로 다르면 -1(혼합)을 반환한다.
		// 낮음으로 잘못 표기하지 않도록 별도 표기한다.
		if (Quality < 0)
		{
			return FText::FromString(TEXT("사용자 지정"));
		}
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

	/**
	 * 리바인드 가능한 액션 정의 목록. IA_Move/IA_Look처럼 축 입력(WASD, 마우스)은
	 * 단일 키 리바인드 UI에 맞지 않아 제외한다.
	 * LabelWidgetName/RebindButtonWidgetName에 해당하는 위젯이 WBP_SettingsPage_Controls에
	 * 없으면 FindWidget이 안전하게 no-op하므로, 행을 WBP에 추가하는 즉시 자동으로 노출된다.
	 */
	struct FRebindActionDef
	{
		const TCHAR* ActionAssetName;
		const TCHAR* AssetPath;
		const TCHAR* MappingName;
		const TCHAR* DisplayLabel;
		const TCHAR* KeyTextWidgetName;
		const TCHAR* RebindButtonWidgetName;
	};

	const TArray<FRebindActionDef>& GetRebindActionDefs()
	{
		// 공격(IA_Attack)은 목록에서 제외: 마우스 좌클릭 고정(조작키 안내의 마우스 아이콘 표기를
		// 바꿀 수 없어 리바인드를 막는다). Key_Attack 행은 RefreshKeyBindings에서 "고정" 표기.
		static const TArray<FRebindActionDef> Defs = {
			{ TEXT("IA_Roll"),         TEXT("/Game/Retrieve/Input/Actions/IA_Roll.IA_Roll"),                 TEXT("Retrieve.Dodge"),       TEXT("회피"),     TEXT("Key_Dodge"),        TEXT("KeyBtn_Dodge") },
			{ TEXT("IA_LockOn"),       TEXT("/Game/Retrieve/Input/Actions/IA_LockOn.IA_LockOn"),             TEXT("Retrieve.LockOn"),      TEXT("락온"),     TEXT("Key_LockOn"),       TEXT("KeyBtn_LockOn") },
			{ TEXT("IA_Sprint"),       TEXT("/Game/Retrieve/Input/Actions/IA_Sprint.IA_Sprint"),             TEXT("Retrieve.Sprint"),      TEXT("질주"),     TEXT("Key_Sprint"),       TEXT("KeyBtn_Sprint") },
			{ TEXT("IA_Jump"),         TEXT("/Game/Retrieve/Input/Actions/IA_Jump.IA_Jump"),                 TEXT("Retrieve.Jump"),        TEXT("점프"),     TEXT("Key_Jump"),         TEXT("KeyBtn_Jump") },
			{ TEXT("IA_Interaction"),  TEXT("/Game/Retrieve/Input/Actions/IA_Interaction.IA_Interaction"),   TEXT("Retrieve.Interact"),    TEXT("상호작용"), TEXT("Key_Interaction"),  TEXT("KeyBtn_Interaction") },
			{ TEXT("IA_HeavyAttack"),  TEXT("/Game/Retrieve/Input/Actions/IA_HeavyAttack.IA_HeavyAttack"),   TEXT("Retrieve.HeavyAttack"), TEXT("강공격"),   TEXT("Key_HeavyAttack"),  TEXT("KeyBtn_HeavyAttack") },
			{ TEXT("IA_Guard"),        TEXT("/Game/Retrieve/Input/Actions/IA_Guard.IA_Guard"),               TEXT("Retrieve.Guard"),       TEXT("가드"),     TEXT("Key_Guard"),        TEXT("KeyBtn_Guard") },
			{ TEXT("IA_Crouch"),       TEXT("/Game/Retrieve/Input/Actions/IA_Crouch.IA_Crouch"),             TEXT("Retrieve.Crouch"),      TEXT("웅크리기"), TEXT("Key_Crouch"),       TEXT("KeyBtn_Crouch") },
			{ TEXT("IA_Burst"),        TEXT("/Game/Retrieve/Input/Actions/IA_Burst.IA_Burst"),               TEXT("Retrieve.Burst"),       TEXT("버스트"),   TEXT("Key_Burst"),        TEXT("KeyBtn_Burst") },
			{ TEXT("IA_Absorb"),       TEXT("/Game/Retrieve/Input/Actions/IA_Absorb.IA_Absorb"),             TEXT("Retrieve.Absorb"),      TEXT("흡수"),     TEXT("Key_Absorb"),       TEXT("KeyBtn_Absorb") },
			{ TEXT("IA_RecallLumen"),  TEXT("/Game/Retrieve/Input/Actions/IA_RecallLumen.IA_RecallLumen"),   TEXT("Retrieve.RecallLumen"), TEXT("루멘 소환"),TEXT("Key_RecallLumen"),  TEXT("KeyBtn_RecallLumen") },
			{ TEXT("IA_ElementMode1"), TEXT("/Game/Retrieve/Input/Actions/IA_ElementMode1.IA_ElementMode1"), TEXT("Retrieve.Element1"),    TEXT("원소 1"),   TEXT("Key_Element1"),     TEXT("KeyBtn_Element1") },
			{ TEXT("IA_ElementMode2"), TEXT("/Game/Retrieve/Input/Actions/IA_ElementMode2.IA_ElementMode2"), TEXT("Retrieve.Element2"),    TEXT("원소 2"),   TEXT("Key_Element2"),     TEXT("KeyBtn_Element2") },
			{ TEXT("IA_ElementMode3"), TEXT("/Game/Retrieve/Input/Actions/IA_ElementMode3.IA_ElementMode3"), TEXT("Retrieve.Element3"),    TEXT("원소 3"),   TEXT("Key_Element3"),     TEXT("KeyBtn_Element3") },
		};
		return Defs;
	}

	/**
	 * 트리거(짧게/길게)만 다르고 같은 물리 키를 공유하도록 설계된 액션 그룹.
	 * 그룹 내 한 액션을 리바인드하면 나머지도 같은 키로 함께 이동하고, 서로는 중복 검사에서 제외된다.
	 * 현재: 회피(짧게)/질주(길게) = Shift.
	 */
	TArray<FName> GetLinkedRebindActionNames(const FName ActionAssetName)
	{
		static const TArray<TArray<FName>> Groups = {
			{ FName(TEXT("IA_Roll")), FName(TEXT("IA_Sprint")) },
		};
		for (const TArray<FName>& Group : Groups)
		{
			if (Group.Contains(ActionAssetName))
			{
				return Group;
			}
		}
		return { ActionAssetName };
	}

	/** 연동 그룹 액션의 키 표기 뒤에 붙는 트리거 구분(같은 키가 두 행에 보이는 이유를 설명). */
	FString GetRebindDisplaySuffix(const FName ActionAssetName)
	{
		if (ActionAssetName == TEXT("IA_Roll"))   return TEXT(" (짧게)");
		if (ActionAssetName == TEXT("IA_Sprint")) return TEXT(" (길게)");
		return FString();
	}

	/** 액션 에셋 이름으로 활성 프로필에서 키보드 매핑 이름을 찾는다. 없으면 NAME_None. */
	FName FindMappingNameForAction(const UEnhancedPlayerMappableKeyProfile* Profile, const FName ActionAssetName)
	{
		if (!Profile)
		{
			return NAME_None;
		}
		for (const TPair<FName, FKeyMappingRow>& Pair : Profile->GetPlayerMappingRows())
		{
			for (const FPlayerKeyMapping& Mapping : Pair.Value.Mappings)
			{
				const UInputAction* Action = Mapping.GetAssociatedInputAction();
				if (Action && Action->GetFName() == ActionAssetName && !Mapping.GetCurrentKey().IsGamepadKey())
				{
					return Pair.Key;
				}
			}
		}
		return NAME_None;
	}

	void CopyHBoxSlot(const UHorizontalBoxSlot* From, UHorizontalBoxSlot* To)
	{
		if (!From || !To) return;
		To->SetSize(From->GetSize());
		To->SetHorizontalAlignment(From->GetHorizontalAlignment());
		To->SetVerticalAlignment(From->GetVerticalAlignment());
		To->SetPadding(From->GetPadding());
	}

	void CopyVBoxSlot(const UVerticalBoxSlot* From, UVerticalBoxSlot* To)
	{
		if (!From || !To) return;
		To->SetSize(From->GetSize());
		To->SetHorizontalAlignment(From->GetHorizontalAlignment());
		To->SetVerticalAlignment(From->GetVerticalAlignment());
		To->SetPadding(From->GetPadding());
	}

	/** 키 재설정 버튼 폭에 맞도록 마우스/보조키 표시 이름을 짧은 표기로 바꾼다. */
	FString ShortKeyDisplayName(const FKey& Key)
	{
		if (Key == EKeys::LeftMouseButton)   return TEXT("좌클릭");
		if (Key == EKeys::RightMouseButton)  return TEXT("우클릭");
		if (Key == EKeys::MiddleMouseButton) return TEXT("휠클릭");
		if (Key == EKeys::LeftShift || Key == EKeys::RightShift)     return TEXT("Shift");
		if (Key == EKeys::LeftControl || Key == EKeys::RightControl) return TEXT("Ctrl");
		if (Key == EKeys::LeftAlt || Key == EKeys::RightAlt)         return TEXT("Alt");
		if (Key == EKeys::SpaceBar)  return TEXT("Space");
		if (Key == EKeys::CapsLock)  return TEXT("Caps");
		if (Key == EKeys::BackSpace) return TEXT("Back");
		return Key.GetDisplayName().ToString();
	}

	// 조작 페이지 스타일 정규화.
	// 1) 활 조준 방식 라벨/설명: 툴로 추가되며 엔진 기본(큰 흰 글씨)이라 On/Off 버튼을 침범 →
	//    Y축 반전 행의 폰트/색을 복사해 통일하고 설명을 On/Off 의미에 맞게 바꾼다.
	// 2) 키 재설정 버튼: 기존 행과 런타임 생성 행의 틴트가 달라 보이므로 전 행을 흰색(기본)으로,
	//    버튼 폭도 하나로 통일해 키 이름이 프레임을 넘지 않게 한다.
	void NormalizeControlsPageStyle(UUserWidget* Page)
	{
		if (!Page)
		{
			return;
		}

		if (const UTextBlock* RefLbl = FindWidget<UTextBlock>(Page, TEXT("Lbl_InvertY")))
		{
			if (UTextBlock* Lbl = FindWidget<UTextBlock>(Page, TEXT("Lbl_BowAimToggle")))
			{
				Lbl->SetFont(RefLbl->GetFont());
				Lbl->SetColorAndOpacity(RefLbl->GetColorAndOpacity());
			}
		}
		if (const UTextBlock* RefDsc = FindWidget<UTextBlock>(Page, TEXT("Dsc_InvertY")))
		{
			if (UTextBlock* Dsc = FindWidget<UTextBlock>(Page, TEXT("Dsc_BowAimToggle")))
			{
				Dsc->SetFont(RefDsc->GetFont());
				Dsc->SetColorAndOpacity(RefDsc->GetColorAndOpacity());
				Dsc->SetText(FText::FromString(TEXT("On: 한 번 클릭으로 조준 유지 / Off: 우클릭 누르는 동안 조준")));
			}
		}

		// 버튼 폭: 템플릿(KB_Dodge)에 고정폭이 있으면 그 값을, 없으면 240을 전 행에 적용한다.
		float ButtonWidth = 240.f;
		if (const USizeBox* TemplateKB = FindWidget<USizeBox>(Page, TEXT("KB_Dodge")))
		{
			if (TemplateKB->GetWidthOverride() > 0.f)
			{
				ButtonWidth = TemplateKB->GetWidthOverride();
			}
		}

		auto NormalizeRow = [Page, ButtonWidth](const TCHAR* ButtonName, const TCHAR* KeyTextName)
		{
			if (UButton* Button = FindWidget<UButton>(Page, ButtonName))
			{
				Button->SetColorAndOpacity(FLinearColor::White);
				Button->SetBackgroundColor(FLinearColor::White);
				Button->SetRenderOpacity(1.f);
			}
			if (UTextBlock* KeyText = FindWidget<UTextBlock>(Page, KeyTextName))
			{
				if (USizeBox* KB = Cast<USizeBox>(KeyText->GetParent()))
				{
					KB->SetWidthOverride(ButtonWidth);
				}
			}
		};
		NormalizeRow(TEXT("KeyBtn_Attack"), TEXT("Key_Attack"));
		for (const FRebindActionDef& Def : GetRebindActionDefs())
		{
			NormalizeRow(Def.RebindButtonWidgetName, Def.KeyTextWidgetName);
		}
	}

	// WBP_SettingsPage_Controls에 아직 배치되지 않은 리바인드 행을 런타임에 생성한다.
	// 기존 회피 행(Row_Key_Dodge)을 템플릿 삼아 폰트/버튼 스타일/슬롯 배치를 그대로 복사하므로
	// 디자인이 유지되고, Key_*, KeyBtn_* 이름 규약 덕에 기존 배선(BindPageEvents)과
	// 표기 갱신(RefreshKeyBindings)이 그대로 동작한다. WBP에 실제 행을 배치하면 그 행이 우선한다.
	void EnsureRebindRows(UUserWidget* Page)
	{
		if (!Page || !Page->WidgetTree)
		{
			return;
		}

		UHorizontalBox* TemplateRow = FindWidget<UHorizontalBox>(Page, TEXT("Row_Key_Dodge"));
		UTextBlock* TemplateLabel = FindWidget<UTextBlock>(Page, TEXT("Lbl_KeyDodge"));
		UButton* TemplateButton = FindWidget<UButton>(Page, TEXT("KeyBtn_Dodge"));
		USizeBox* TemplateSizeBox = FindWidget<USizeBox>(Page, TEXT("KB_Dodge"));
		UTextBlock* TemplateKeyText = FindWidget<UTextBlock>(Page, TEXT("Key_Dodge"));
		UVerticalBox* List = TemplateRow ? Cast<UVerticalBox>(TemplateRow->GetParent()) : nullptr;
		if (!TemplateRow || !TemplateLabel || !TemplateButton || !TemplateKeyText || !List)
		{
			return;
		}

		UWidgetTree* Tree = Page->WidgetTree;
		bool bAddedAny = false;
		for (const FRebindActionDef& Def : GetRebindActionDefs())
		{
			if (FindWidget<UTextBlock>(Page, Def.KeyTextWidgetName))
			{
				continue; // WBP에 이미 배치된 행
			}
			bAddedAny = true;

			UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(
				UHorizontalBox::StaticClass(), FName(*(FString(TEXT("Row_")) + Def.KeyTextWidgetName)));

			// 왼쪽: 액션 이름 라벨(템플릿 폰트/색 복사)
			UVerticalBox* LabelBox = Tree->ConstructWidget<UVerticalBox>();
			UTextBlock* Label = Tree->ConstructWidget<UTextBlock>();
			Label->SetText(FText::FromString(Def.DisplayLabel));
			Label->SetFont(TemplateLabel->GetFont());
			Label->SetColorAndOpacity(TemplateLabel->GetColorAndOpacity());
			UVerticalBoxSlot* LabelSlot = LabelBox->AddChildToVerticalBox(Label);
			CopyVBoxSlot(Cast<UVerticalBoxSlot>(TemplateLabel->Slot), LabelSlot);
			CopyHBoxSlot(Cast<UHorizontalBoxSlot>(TemplateLabel->GetParent()->Slot),
				Row->AddChildToHorizontalBox(LabelBox));

			// 오른쪽: 키 버튼(템플릿 버튼 스타일/크기 복사)
			UButton* Button = Tree->ConstructWidget<UButton>(
				UButton::StaticClass(), FName(Def.RebindButtonWidgetName));
			Button->SetStyle(TemplateButton->GetStyle());

			USizeBox* SizeBox = Tree->ConstructWidget<USizeBox>();
			if (TemplateSizeBox)
			{
				if (TemplateSizeBox->GetWidthOverride() > 0.f)
				{
					SizeBox->SetWidthOverride(TemplateSizeBox->GetWidthOverride());
				}
				if (TemplateSizeBox->GetHeightOverride() > 0.f)
				{
					SizeBox->SetHeightOverride(TemplateSizeBox->GetHeightOverride());
				}
				if (TemplateSizeBox->GetMinDesiredWidth() > 0.f)
				{
					SizeBox->SetMinDesiredWidth(TemplateSizeBox->GetMinDesiredWidth());
				}
			}

			UTextBlock* KeyText = Tree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), FName(Def.KeyTextWidgetName));
			KeyText->SetFont(TemplateKeyText->GetFont());
			KeyText->SetColorAndOpacity(TemplateKeyText->GetColorAndOpacity());
			KeyText->SetJustification(ETextJustify::Center);
			KeyText->SetText(FText::FromString(TEXT("미지정")));

			SizeBox->AddChild(KeyText);
			if (USizeBoxSlot* FromSlot = Cast<USizeBoxSlot>(TemplateKeyText->Slot))
			{
				if (USizeBoxSlot* ToSlot = Cast<USizeBoxSlot>(KeyText->Slot))
				{
					ToSlot->SetPadding(FromSlot->GetPadding());
					ToSlot->SetHorizontalAlignment(FromSlot->GetHorizontalAlignment());
					ToSlot->SetVerticalAlignment(FromSlot->GetVerticalAlignment());
				}
			}
			Button->AddChild(SizeBox);
			if (TemplateSizeBox)
			{
				if (const UButtonSlot* FromSlot = Cast<UButtonSlot>(TemplateSizeBox->Slot))
				{
					if (UButtonSlot* ToSlot = Cast<UButtonSlot>(SizeBox->Slot))
					{
						ToSlot->SetPadding(FromSlot->GetPadding());
						ToSlot->SetHorizontalAlignment(FromSlot->GetHorizontalAlignment());
						ToSlot->SetVerticalAlignment(FromSlot->GetVerticalAlignment());
					}
				}
			}
			CopyHBoxSlot(Cast<UHorizontalBoxSlot>(TemplateButton->Slot),
				Row->AddChildToHorizontalBox(Button));

			// 리스트에 추가(행 간 여백은 템플릿 행 슬롯에서 복사)
			CopyVBoxSlot(Cast<UVerticalBoxSlot>(TemplateRow->Slot), List->AddChildToVerticalBox(Row));
		}

		// "활 조준 방식" 행이 리바인드 행들 뒤에 오도록 맨 끝으로 옮긴다(슬롯 속성 보존).
		if (bAddedAny)
		{
			UWidget* BowRow = Page->GetWidgetFromName(TEXT("Row_BowAimToggle"));
			if (BowRow && BowRow->GetParent() == List)
			{
				const UVerticalBoxSlot* OldSlot = Cast<UVerticalBoxSlot>(BowRow->Slot);
				const FSlateChildSize Size = OldSlot ? OldSlot->GetSize() : FSlateChildSize();
				const EHorizontalAlignment HAlign = OldSlot ? OldSlot->GetHorizontalAlignment() : HAlign_Fill;
				const EVerticalAlignment VAlign = OldSlot ? OldSlot->GetVerticalAlignment() : VAlign_Fill;
				const FMargin Padding = OldSlot ? OldSlot->GetPadding() : FMargin(0.f);

				List->RemoveChild(BowRow);
				if (UVerticalBoxSlot* NewSlot = List->AddChildToVerticalBox(BowRow))
				{
					NewSlot->SetSize(Size);
					NewSlot->SetHorizontalAlignment(HAlign);
					NewSlot->SetVerticalAlignment(VAlign);
					NewSlot->SetPadding(Padding);
				}
			}
		}
	}
}

void URetrieveSettingsPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindScreenEvents();
	BindPageEvents();
	ApplyRuntimeStyle();
	ApplyOptionAvailability();

	// 리바인드 허용 키 판정이 조작키 안내 다이어그램에 실제 그려진 키 기준으로 동작하도록 캐시를 구축한다.
	URetrieveUISettingsLibrary::EnsureControlsGuideKeyCache(GetOwningPlayer());

	// 접근성(고대비) 등 설정 변경 시 화면 스타일을 즉시 갱신하도록 구독한다.
	if (URetrieveSettingsSubsystem* Subsystem = GetSettingsSubsystem())
	{
		Subsystem->OnSettingChanged.AddUniqueDynamic(this, &URetrieveSettingsPanelWidget::HandleSettingChanged);
	}

	// 화면을 연 시점의 확정값을 baseline으로 보관한다(Apply 없이 닫으면 여기로 원복).
	if (const URetrieveGameUserSettings* S = GetUserSettings())
	{
		BaselineSnapshot.CaptureFrom(S);
	}

	// "강제 리스폰" 버튼은 인게임에서만 노출(메인 메뉴 설정에선 숨김)
	if (UButton* RespawnButton = FindWidget<UButton>(this, TEXT("Btn_Respawn")))
	{
		const ARetrieveGameState* GS = GetWorld() ? GetWorld()->GetGameState<ARetrieveGameState>() : nullptr;
		const bool bInGame = GS && GS->GetSessionState() == ERetrieveSessionState::InGame;
		RespawnButton->SetVisibility(bInGame ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	SelectCategory(ERetrieveSettingsCategory::Graphics);
}

void URetrieveSettingsPanelWidget::NativeDestruct()
{
	if (URetrieveSettingsSubsystem* Subsystem = GetSettingsSubsystem())
	{
		Subsystem->OnSettingChanged.RemoveDynamic(this, &URetrieveSettingsPanelWidget::HandleSettingChanged);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RebindWarningTimerHandle);
	}

	if (bAwaitingResolutionConfirm)
	{
		// 확인 없이 닫음 → 안전하게 이전 표시 모드로 복구한다(RevertResolution이 팝업도 정리).
		RevertResolution();
	}
	else
	{
		// Apply하지 않은 프리뷰 변경을 baseline으로 원복한다(런타임만, ini는 마지막 Apply 상태 유지).
		RevertToBaseline();
	}
	HideResolutionConfirmPopup();

	Super::NativeDestruct();
}

void URetrieveSettingsPanelWidget::RevertToBaseline()
{
	URetrieveGameUserSettings* S = GetUserSettings();
	if (!S)
	{
		return;
	}
	BaselineSnapshot.RestoreTo(S);
	if (URetrieveSettingsSubsystem* Subsystem = GetSettingsSubsystem())
	{
		Subsystem->ApplyAllSettings(/*bSaveToDisk*/ false);
	}
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

	// 연동 그룹(회피/질주)은 함께 이동하므로 그룹 전체를 중복 검사에서 제외한다.
	const TArray<FName> LinkedActions = GetLinkedRebindActionNames(PendingActionAssetName);

	for (const TPair<FName, FKeyMappingRow>& Pair : Profile->GetPlayerMappingRows())
	{
		if (Pair.Key == PendingMappingName) continue;
		for (const FPlayerKeyMapping& Mapping : Pair.Value.Mappings)
		{
			const UInputAction* Action = Mapping.GetAssociatedInputAction();
			if (Action && LinkedActions.Contains(Action->GetFName())) continue;
			if (Mapping.GetCurrentKey() == Key && !Mapping.GetCurrentKey().IsGamepadKey())
			{
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

void URetrieveSettingsPanelWidget::CancelRebindWithWarning(const FString& Message)
{
	SetText(GetPage(ERetrieveSettingsCategory::Controls), PendingKeyLabelName, FText::FromString(Message));

	// 이전 타이머 초기화 후 2초 뒤 원래 키 표기로 복원
	GetWorld()->GetTimerManager().ClearTimer(RebindWarningTimerHandle);
	GetWorld()->GetTimerManager().SetTimer(RebindWarningTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			RefreshKeyBindings();
		}),
		2.0f, false);

	PendingMappingName = NAME_None;
	PendingActionAssetName = NAME_None;
	PendingKeyLabelName = NAME_None;
}

void URetrieveSettingsPanelWidget::FinishRebindWithKey(const FKey& NewKey)
{
	if (NewKey == EKeys::Escape)
	{
		PendingMappingName = NAME_None;
		PendingActionAssetName = NAME_None;
		PendingKeyLabelName = NAME_None;
		RefreshKeyBindings();
		return;
	}

	if (!NewKey.IsValid() || NewKey.IsGamepadKey())
	{
		return;
	}

	// 마우스 입력은 할당 금지: 조작키 안내의 마우스 다이어그램은 아이콘·문구가 고정이라 반영할 수 없다.
	if (NewKey.IsMouseButton())
	{
		CancelRebindWithWarning(TEXT("⚠ 마우스 키는 할당 불가"));
		return;
	}

	// 조작키 안내 키보드 다이어그램에 슬롯이 있는 키만 허용한다(설정↔안내 항상 일치).
	if (!URetrieveUISettingsLibrary::IsControlsGuideDisplayableKey(NewKey))
	{
		CancelRebindWithWarning(TEXT("⚠ 사용할 수 없는 키"));
		return;
	}

	// 중복 키 검사
	const FName ConflictAction = GetConflictingAction(NewKey);
	if (!ConflictAction.IsNone())
	{
		CancelRebindWithWarning(FString::Printf(TEXT("⚠ 이미 사용 중: %s"), *ActionToKorean(ConflictAction)));
		return;
	}

	if (UEnhancedInputUserSettings* InputSettings = GetEnhancedInputUserSettings())
	{
		// 연동 그룹(회피/질주 = Shift 짧게/길게)은 같은 키로 함께 이동한다.
		// 트리거(Tap/Hold)는 IMC 매핑 쪽에 있으므로 키만 바꿔도 짧게/길게 동작이 유지된다.
		const UEnhancedPlayerMappableKeyProfile* Profile = InputSettings->GetActiveKeyProfile();
		bool bAnyMapped = false;
		for (const FName& LinkedAction : GetLinkedRebindActionNames(PendingActionAssetName))
		{
			const FName MappingName = (LinkedAction == PendingActionAssetName)
				? PendingMappingName
				: FindMappingNameForAction(Profile, LinkedAction);
			if (MappingName.IsNone())
			{
				continue;
			}

			FMapPlayerKeyArgs Args;
			Args.MappingName = MappingName;
			Args.Slot = EPlayerMappableKeySlot::First;
			Args.NewKey = NewKey;
			Args.bCreateMatchingSlotIfNeeded = true;
			FGameplayTagContainer FailureReason;
			InputSettings->MapPlayerKey(Args, FailureReason);
			bAnyMapped |= FailureReason.IsEmpty();
		}
		if (bAnyMapped)
		{
			InputSettings->ApplySettings();
			InputSettings->AsyncSaveSettings();
		}
	}
	PendingMappingName = NAME_None;
	PendingActionAssetName = NAME_None;
	PendingKeyLabelName = NAME_None;
	// 연동 그룹의 다른 행 표기까지 함께 갱신되도록 전체 목록을 다시 그린다.
	RefreshKeyBindings();
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
	URetrieveGameUserSettings* S = GetUserSettings();
	URetrieveSettingsSubsystem* Subsystem = GetSettingsSubsystem();
	if (!S || !Subsystem)
	{
		OnSettingsApplied();
		return;
	}

	// Apply 직전 값(원복 기준). baseline과 표시 모드가 다르면 확인 트랜잭션을 건다.
	FRetrieveSettingsSnapshot PendingState;
	PendingState.CaptureFrom(S);
	const bool bDisplayModeChanged =
		bEnableResolutionConfirm && PendingState.DiffersInDisplayMode(BaselineSnapshot);
	if (bDisplayModeChanged)
	{
		PreConfirmSnapshot = BaselineSnapshot;
	}

	Subsystem->ApplyAllSettings(/*bSaveToDisk*/ true);

	// 새 확정값을 baseline으로 갱신(이후 닫아도 원복되지 않음).
	BaselineSnapshot.CaptureFrom(S);

	if (bDisplayModeChanged)
	{
		bAwaitingResolutionConfirm = true;
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ResolutionConfirmTimerHandle);
			World->GetTimerManager().SetTimer(ResolutionConfirmTimerHandle,
				FTimerDelegate::CreateWeakLambda(this, [this]() { RevertResolution(); }),
				FMath::Max(1.f, ResolutionConfirmTimeoutSeconds), false);
		}

		// 팝업 클래스가 지정돼 있으면 C++가 직접 띄우고 배선한다. 없으면 BP 이벤트로 위임.
		if (ResolutionConfirmPopupClass)
		{
			ShowResolutionConfirmPopup(ResolutionConfirmTimeoutSeconds);
		}
		else
		{
			OnResolutionConfirmRequested(ResolutionConfirmTimeoutSeconds);
		}
	}

	OnSettingsApplied();
}

void URetrieveSettingsPanelWidget::ConfirmResolution()
{
	if (!bAwaitingResolutionConfirm)
	{
		return;
	}
	bAwaitingResolutionConfirm = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ResolutionConfirmTimerHandle);
	}
	HideResolutionConfirmPopup();
	// Apply 단계에서 이미 저장 + baseline 갱신됨. 새 해상도를 그대로 확정한다.
	OnResolutionConfirmResolved();
}

void URetrieveSettingsPanelWidget::RevertResolution()
{
	if (!bAwaitingResolutionConfirm)
	{
		return;
	}
	bAwaitingResolutionConfirm = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ResolutionConfirmTimerHandle);
	}

	if (URetrieveGameUserSettings* S = GetUserSettings())
	{
		// 표시 모드만 직전 값으로 복구(다른 적용값은 유지).
		S->SetScreenResolution(PreConfirmSnapshot.Resolution);
		S->SetFullscreenMode(static_cast<EWindowMode::Type>(PreConfirmSnapshot.WindowMode));
		if (URetrieveSettingsSubsystem* Subsystem = GetSettingsSubsystem())
		{
			Subsystem->ApplyCategory(ERetrieveSettingsCategory::Graphics, /*bSaveToDisk*/ true);
		}
		BaselineSnapshot.CaptureFrom(S);
	}
	HideResolutionConfirmPopup();
	RefreshCurrentPage();
	OnResolutionConfirmResolved();
}

void URetrieveSettingsPanelWidget::ShowResolutionConfirmPopup(float TimeoutSeconds)
{
	if (!ResolutionConfirmPopupClass)
	{
		return;
	}

	HideResolutionConfirmPopup(); // 혹시 남아 있으면 정리

	ActiveResolutionPopup = CreateWidget<UUserWidget>(GetOwningPlayer(), ResolutionConfirmPopupClass);
	if (!ActiveResolutionPopup)
	{
		return;
	}
	ActiveResolutionPopup->AddToViewport(/*ZOrder*/ 1000);

	// 메시지 + 버튼 배선(이름으로 찾는다 — 없으면 안전하게 무시).
	SetText(ActiveResolutionPopup, TEXT("Txt_Message"),
		FText::FromString(TEXT("변경된 해상도를 유지할까요?")));

	if (UButton* KeepButton = FindWidget<UButton>(ActiveResolutionPopup, TEXT("Btn_Keep")))
	{
		KeepButton->OnClicked.AddUniqueDynamic(this, &URetrieveSettingsPanelWidget::ConfirmResolution);
		RegisterSoundButton(KeepButton);
	}
	if (UButton* RevertButton = FindWidget<UButton>(ActiveResolutionPopup, TEXT("Btn_Revert")))
	{
		RevertButton->OnClicked.AddUniqueDynamic(this, &URetrieveSettingsPanelWidget::RevertResolution);
		RegisterSoundButton(RevertButton);
	}

	// 카운트다운 표시: 0.25초마다 남은 시간을 갱신.
	if (UWorld* World = GetWorld())
	{
		ResolutionConfirmEndTime = World->GetTimeSeconds() + FMath::Max(1.f, TimeoutSeconds);
		UpdateResolutionCountdown();
		World->GetTimerManager().ClearTimer(ResolutionCountdownTimerHandle);
		World->GetTimerManager().SetTimer(ResolutionCountdownTimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [this]() { UpdateResolutionCountdown(); }),
			0.25f, true);
	}
}

void URetrieveSettingsPanelWidget::HideResolutionConfirmPopup()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ResolutionCountdownTimerHandle);
	}
	if (ActiveResolutionPopup)
	{
		ActiveResolutionPopup->RemoveFromParent();
		ActiveResolutionPopup = nullptr;
	}
}

void URetrieveSettingsPanelWidget::UpdateResolutionCountdown()
{
	if (!ActiveResolutionPopup)
	{
		return;
	}
	const UWorld* World = GetWorld();
	const double Remaining = World ? FMath::Max(0.0, ResolutionConfirmEndTime - World->GetTimeSeconds()) : 0.0;
	SetText(ActiveResolutionPopup, TEXT("Txt_Countdown"),
		FText::FromString(FString::Printf(TEXT("%d초 후 자동 복구"), FMath::CeilToInt(Remaining))));
}

void URetrieveSettingsPanelWidget::ResetCurrentCategory()
{
	if (URetrieveSettingsSubsystem* Subsystem = GetSettingsSubsystem())
	{
		Subsystem->ResetCategory(CurrentCategory);
	}

	// 키맵핑은 GameUserSettings가 아니라 EnhancedInputUserSettings에 저장되므로 별도로 복원한다.
	if (CurrentCategory == ERetrieveSettingsCategory::Controls)
	{
		ResetKeyBindingsToDefaults();
	}

	RefreshCurrentPage();
}

void URetrieveSettingsPanelWidget::ResetKeyBindingsToDefaults()
{
	UEnhancedInputUserSettings* InputSettings = GetEnhancedInputUserSettings();
	const UEnhancedPlayerMappableKeyProfile* Profile = InputSettings ? InputSettings->GetActiveKeyProfile() : nullptr;
	if (!Profile)
	{
		return;
	}

	// 리바인드 대기 상태가 남아 있으면 정리한다(복원 후 옛 대기 상태로 키가 씌워지는 것 방지).
	PendingMappingName = NAME_None;
	PendingActionAssetName = NAME_None;
	PendingKeyLabelName = NAME_None;

	// 수정하면서 순회하지 않도록 복원 대상을 먼저 수집한다.
	TArray<TPair<FName, FKey>> ToRestore;
	for (const TPair<FName, FKeyMappingRow>& Pair : Profile->GetPlayerMappingRows())
	{
		for (const FPlayerKeyMapping& Mapping : Pair.Value.Mappings)
		{
			if (!Mapping.GetCurrentKey().IsGamepadKey() && Mapping.IsCustomized())
			{
				ToRestore.Emplace(Pair.Key, Mapping.GetDefaultKey());
			}
		}
	}
	if (ToRestore.IsEmpty())
	{
		return;
	}

	for (const TPair<FName, FKey>& Restore : ToRestore)
	{
		FMapPlayerKeyArgs Args;
		Args.MappingName = Restore.Key;
		Args.Slot = EPlayerMappableKeySlot::First;
		Args.NewKey = Restore.Value;
		FGameplayTagContainer FailureReason;
		InputSettings->MapPlayerKey(Args, FailureReason);
	}
	InputSettings->ApplySettings();
	InputSettings->AsyncSaveSettings();
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
	BIND_SCREEN_BUTTON("Btn_Respawn", HandleRespawn)
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
#define BIND_PAGE_TOGGLE(Category, ToggleHandler) \
	{ UUserWidget* _P = GetPage(Category); if (_P && _P->WidgetTree) _P->WidgetTree->ForEachWidget([this](UWidget* _W) { \
	  if (URetrieveSettingRowToggle* _T = Cast<URetrieveSettingRowToggle>(_W)) \
	  { _T->OnRowToggleChanged.AddUniqueDynamic(this, &ThisClass::ToggleHandler); } }); }

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
	BIND_PAGE_TOGGLE(C::Graphics, HandleGraphicsToggleChanged)
	BIND_PAGE_SLIDER(C::Graphics, "Sld_FrameLimit", HandleFrameLimitChanged)
	BIND_PAGE_SLIDER(C::Graphics, "Sld_Gamma", HandleGammaChanged)

	BIND_PAGE_SLIDER(C::Controls, "Sld_MouseX", HandleMouseXChanged)
	BIND_PAGE_SLIDER(C::Controls, "Sld_MouseY", HandleMouseYChanged)
	BIND_PAGE_SLIDER(C::Controls, "Sld_PadSens", HandlePadSensitivityChanged)
	BIND_PAGE_CHECK(C::Controls, "Chk_InvertY", HandleInvertYChanged)
	BIND_PAGE_CHECK(C::Controls, "Chk_Vibration", HandleVibrationChanged)
	BIND_PAGE_TOGGLE(C::Controls, HandleControlsToggleChanged)
	BIND_PAGE_BUTTON(C::Controls, "Btn_LockOn_Prev", HandleLockOnPrev)
	BIND_PAGE_BUTTON(C::Controls, "Btn_LockOn_Next", HandleLockOnNext)
	// 공격은 마우스 좌클릭 고정: 리바인드 버튼을 비활성화한다(행은 참고용으로 남김).
	if (UButton* AttackButton = FindWidget<UButton>(GetPage(C::Controls), TEXT("KeyBtn_Attack")))
	{
		AttackButton->SetIsEnabled(false);
	}
	// WBP에 아직 배치되지 않은 리바인드 행을 템플릿(회피 행) 복제로 생성한다.
	// 아래 KeyBtn_* 배선이 이름으로 찾으므로 반드시 배선 전에 만들어야 한다.
	EnsureRebindRows(GetPage(C::Controls));
	// 활 조준 방식 라벨 폰트, 버튼 틴트/폭을 전 행 통일(기존 행·생성 행 색상 차이 제거).
	NormalizeControlsPageStyle(GetPage(C::Controls));
	BIND_PAGE_BUTTON(C::Controls, "KeyBtn_Dodge", HandleRebindDodge)
	BIND_PAGE_BUTTON(C::Controls, "KeyBtn_LockOn", HandleRebindLockOn)
	// 아래는 WBP에 해당 Row_Key_*/KeyBtn_* 위젯이 추가되는 즉시 자동으로 동작한다(없으면 안전하게 no-op).
	BIND_PAGE_BUTTON(C::Controls, "KeyBtn_Sprint", HandleRebindSprint)
	BIND_PAGE_BUTTON(C::Controls, "KeyBtn_Jump", HandleRebindJump)
	BIND_PAGE_BUTTON(C::Controls, "KeyBtn_Interaction", HandleRebindInteract)
	BIND_PAGE_BUTTON(C::Controls, "KeyBtn_HeavyAttack", HandleRebindHeavyAttack)
	BIND_PAGE_BUTTON(C::Controls, "KeyBtn_Guard", HandleRebindGuard)
	BIND_PAGE_BUTTON(C::Controls, "KeyBtn_Crouch", HandleRebindCrouch)
	BIND_PAGE_BUTTON(C::Controls, "KeyBtn_Burst", HandleRebindBurst)
	BIND_PAGE_BUTTON(C::Controls, "KeyBtn_Absorb", HandleRebindAbsorb)
	BIND_PAGE_BUTTON(C::Controls, "KeyBtn_RecallLumen", HandleRebindRecallLumen)
	BIND_PAGE_BUTTON(C::Controls, "KeyBtn_Element1", HandleRebindElement1)
	BIND_PAGE_BUTTON(C::Controls, "KeyBtn_Element2", HandleRebindElement2)
	BIND_PAGE_BUTTON(C::Controls, "KeyBtn_Element3", HandleRebindElement3)

	BIND_PAGE_SLIDER(C::Audio, "Sld_Master", HandleMasterChanged)
	BIND_PAGE_SLIDER(C::Audio, "Sld_Music", HandleMusicChanged)
	BIND_PAGE_SLIDER(C::Audio, "Sld_Sfx", HandleSfxChanged)
	BIND_PAGE_SLIDER(C::Audio, "Sld_Ambience", HandleAmbienceChanged)
	BIND_PAGE_SLIDER(C::Audio, "Sld_UI", HandleUIChanged)
	BIND_PAGE_SLIDER(C::Audio, "Sld_Voice", HandleVoiceChanged)
	BIND_PAGE_CHECK(C::Audio, "Chk_MuteUnfocused", HandleMuteUnfocusedChanged)
	BIND_PAGE_TOGGLE(C::Audio, HandleAudioToggleChanged)

	BIND_PAGE_BUTTON(C::Gameplay, "Btn_Language_Prev", HandleLanguagePrev)
	BIND_PAGE_BUTTON(C::Gameplay, "Btn_Language_Next", HandleLanguageNext)
	BIND_PAGE_CHECK(C::Gameplay, "Chk_Subtitles", HandleSubtitlesChanged)
	BIND_PAGE_CHECK(C::Gameplay, "Chk_DamageNumbers", HandleDamageNumbersChanged)
	BIND_PAGE_CHECK(C::Gameplay, "Chk_TutorialHints", HandleTutorialHintsChanged)
	BIND_PAGE_TOGGLE(C::Gameplay, HandleGameplayToggleChanged)
	BIND_PAGE_SLIDER(C::Gameplay, "Sld_SubtitleScale", HandleSubtitleScaleChanged)
	BIND_PAGE_SLIDER(C::Gameplay, "Sld_FOV", HandleFOVChanged)
	BIND_PAGE_SLIDER(C::Gameplay, "Sld_CameraShake", HandleCameraShakeChanged)

	BIND_PAGE_BUTTON(C::Accessibility, "Btn_ColorBlind_Prev", HandleColorBlindPrev)
	BIND_PAGE_BUTTON(C::Accessibility, "Btn_ColorBlind_Next", HandleColorBlindNext)
	BIND_PAGE_BUTTON(C::Accessibility, "Btn_Interact_Prev", HandleInteractPrev)
	BIND_PAGE_BUTTON(C::Accessibility, "Btn_Interact_Next", HandleInteractNext)
	// 접근성 슬라이더는 WBP_SettingRow_Slider(RowKey)로 전환됨 → 아래 BindAccessibilityRows()에서 구독.
	BIND_PAGE_CHECK(C::Accessibility, "Chk_HighContrast", HandleHighContrastChanged)
	BIND_PAGE_CHECK(C::Accessibility, "Chk_ReduceMotion", HandleReduceMotionChanged)
	BIND_PAGE_TOGGLE(C::Accessibility, HandleAccessibilityToggleChanged)

#undef BIND_PAGE_CHECK
#undef BIND_PAGE_SLIDER
#undef BIND_PAGE_BUTTON
#undef BIND_PAGE_TOGGLE

	// 슬라이더 행 아키타입(Audio) 값 변경 구독.
	BindAudioRows();
	BindGraphicsRows();
	BindControlsRows();
	BindGameplayRows();
	BindAccessibilityRows();
}

void URetrieveSettingsPanelWidget::UpdateScreenForCategory(const ERetrieveSettingsCategory Category)
{
	if (UWidgetSwitcher* Switcher = FindWidget<UWidgetSwitcher>(this, TEXT("Switcher_Pages")))
	{
		Switcher->SetActiveWidgetIndex(static_cast<int32>(Category));
	}

	const URetrieveUITheme* Theme = URetrieveUISettingsLibrary::GetActiveUITheme();
	const FLinearColor DarkChip = Theme ? Theme->PanelBackground : FLinearColor(0.12f, 0.10f, 0.07f, 0.55f);
	const FLinearColor Gold = Theme ? Theme->Accent : FLinearColor(0.80f, 0.62f, 0.28f, 0.90f);
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
		// 테두리 없는 창은 항상 데스크톱 해상도로 동작하므로 실제 적용 값을 보여준다.
		const bool bBorderless = S->GetRetrieveWindowMode() == ERetrieveWindowMode::WindowedFullscreen;
		const FIntPoint Res = bBorderless ? S->GetDesktopResolution() : S->GetScreenResolution();
		SetText(Page, TEXT("Val_Resolution"), FText::FromString(FString::Printf(TEXT("%d x %d"), Res.X, Res.Y)));
	}
	SetText(Page, TEXT("Val_Quality"), QualityText(S->GetOverallScalabilityLevel()));
	SetText(Page, TEXT("Val_Shadow"), QualityText(S->GetShadowQuality()));
	SetText(Page, TEXT("Val_Texture"), QualityText(S->GetTextureQuality()));
	SetText(Page, TEXT("Val_Effects"), QualityText(S->GetVisualEffectQuality()));
	SetChecked(Page, TEXT("Chk_VSync"), S->IsVSyncEnabled());
	SetChecked(Page, TEXT("Chk_MotionBlur"), S->bMotionBlur);
	SetToggleByKey(Page, TEXT("Graphics_VSync"), S->IsVSyncEnabled());
	SetToggleByKey(Page, TEXT("Graphics_MotionBlur"), S->bMotionBlur);
	// WBP_SettingRow_Slider 행은 내부 USlider가 항상 0..1 범위이므로 역매핑 후 SetValueSilently로 갱신
	if (Page->WidgetTree)
	{
		Page->WidgetTree->ForEachWidget([&](UWidget* W)
		{
			if (URetrieveSettingRowSlider* Row = Cast<URetrieveSettingRowSlider>(W))
			{
				if (Row->RowKey == TEXT("Graphics_Gamma"))
					Row->SetValueSilently((S->GammaLevel - 1.8f) / 0.8f);
				else if (Row->RowKey == TEXT("Graphics_FrameLimit"))
					Row->SetValueSilently((S->GetFrameRateLimit() - 30.f) / 210.f);
			}
		});
	}
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
	SetToggleByKey(Page, TEXT("Controls_InvertY"), S->bInvertMouseY);
	SetToggleByKey(Page, TEXT("Controls_Vibration"), S->bGamepadVibration);
	SetText(Page, TEXT("Val_LockOn"), FText::FromString(S->bLockOnToggleMode ? TEXT("토글") : TEXT("홀드")));
	SetToggleByKey(Page, TEXT("Controls_BowAimToggle"), S->bBowAimToggleMode);

	// 슬라이더 행 아키타입(URetrieveSettingRowSlider) 값 갱신. Range는 BindControlsRows에서 이미 설정됨.
	if (Page->WidgetTree)
	{
		Page->WidgetTree->ForEachWidget([S](UWidget* W)
		{
			if (URetrieveSettingRowSlider* Row = Cast<URetrieveSettingRowSlider>(W))
			{
				if      (Row->RowKey == TEXT("Controls_MouseX"))  Row->SetRawValueSilently(S->MouseSensitivityX);
				else if (Row->RowKey == TEXT("Controls_MouseY"))  Row->SetRawValueSilently(S->MouseSensitivityY);
				else if (Row->RowKey == TEXT("Controls_PadSens")) Row->SetRawValueSilently(S->GamepadSensitivityX);
				// 통합 행: X==Y면 그 값, 개별 조절로 달라졌으면 평균을 표시(조절하는 순간 X/Y가 다시 동기화됨).
				else if (Row->RowKey == TEXT("Controls_MouseAll"))
					Row->SetRawValueSilently((S->MouseSensitivityX + S->MouseSensitivityY) * 0.5f);
			}
		});
	}
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
		for (const FRebindActionDef& Def : GetRebindActionDefs())
		{
			EnsurePlayerMappableAction(Def.AssetPath, Def.MappingName, FText::FromString(Def.DisplayLabel));
		}
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

	const UEnhancedPlayerMappableKeyProfile* Profile = InputSettings->GetActiveKeyProfile();
	if (!Profile)
	{
		return;
	}

	// 공격은 마우스 좌클릭 고정. 리바인드 UI가 열려 있던 과거 버전에서 다른 키로 바꿔 저장한
	// 프로필이 남아있을 수 있으므로, 커스터마이즈된 공격 매핑을 발견하면 기본 키로 복원한다.
	for (const TPair<FName, FKeyMappingRow>& Pair : Profile->GetPlayerMappingRows())
	{
		for (const FPlayerKeyMapping& Mapping : Pair.Value.Mappings)
		{
			const UInputAction* Action = Mapping.GetAssociatedInputAction();
			if (Action && Action->GetFName() == TEXT("IA_Attack") && Mapping.IsCustomized())
			{
				FMapPlayerKeyArgs Args;
				Args.MappingName = Pair.Key;
				Args.Slot = EPlayerMappableKeySlot::First;
				Args.NewKey = Mapping.GetDefaultKey();
				FGameplayTagContainer FailureReason;
				InputSettings->MapPlayerKey(Args, FailureReason);
				if (FailureReason.IsEmpty())
				{
					InputSettings->ApplySettings();
					InputSettings->AsyncSaveSettings();
				}
			}
		}
	}
	SetText(Page, TEXT("Key_Attack"), FText::FromString(TEXT("좌클릭 (고정)")));

	for (const FRebindActionDef& Def : GetRebindActionDefs())
	{
		// WBP에 아직 이 행이 없으면 SetText가 안전하게 no-op한다.
		FText Display = FText::FromString(TEXT("미지정"));
		bool bMouseLocked = false;
		for (const TPair<FName, FKeyMappingRow>& Pair : Profile->GetPlayerMappingRows())
		{
			for (const FPlayerKeyMapping& Mapping : Pair.Value.Mappings)
			{
				const UInputAction* Action = Mapping.GetAssociatedInputAction();
				if (Action && Action->GetFName() == FName(Def.ActionAssetName) && !Mapping.GetCurrentKey().IsGamepadKey())
				{
					// 기본키가 마우스인 액션(강공격 등)은 리바인드 잠금: 조작키 안내의 마우스
					// 다이어그램이 고정 표기라 옮겨 그릴 수 없다. 과거 버전에서 다른 키로 바꿔
					// 저장한 프로필이 남아있으면 기본 키로 복원한다(IA_Attack과 동일한 정책).
					if (Mapping.GetDefaultKey().IsMouseButton())
					{
						bMouseLocked = true;
						if (Mapping.IsCustomized())
						{
							FMapPlayerKeyArgs Args;
							Args.MappingName = Pair.Key;
							Args.Slot = EPlayerMappableKeySlot::First;
							Args.NewKey = Mapping.GetDefaultKey();
							FGameplayTagContainer FailureReason;
							InputSettings->MapPlayerKey(Args, FailureReason);
							if (FailureReason.IsEmpty())
							{
								InputSettings->ApplySettings();
								InputSettings->AsyncSaveSettings();
							}
						}
						Display = FText::FromString(
							FString::Printf(TEXT("%s (고정)"), *ShortKeyDisplayName(Mapping.GetDefaultKey())));
					}
					else
					{
						// 연동 그룹(회피/질주)은 같은 키가 두 행에 보이므로 짧게/길게 구분을 덧붙인다.
						Display = FText::FromString(
							ShortKeyDisplayName(Mapping.GetCurrentKey()) +
							GetRebindDisplaySuffix(FName(Def.ActionAssetName)));
					}
					break;
				}
			}
		}
		SetText(Page, Def.KeyTextWidgetName, Display);
		if (UButton* RebindButton = FindWidget<UButton>(Page, Def.RebindButtonWidgetName))
		{
			RebindButton->SetIsEnabled(!bMouseLocked);
		}
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
	PendingActionAssetName = NAME_None;
	FKey MappingDefaultKey;
	for (const TPair<FName, FKeyMappingRow>& Pair : Profile->GetPlayerMappingRows())
	{
		for (const FPlayerKeyMapping& Mapping : Pair.Value.Mappings)
		{
			const UInputAction* Action = Mapping.GetAssociatedInputAction();
			if (Action && Action->GetFName() == ActionAssetName && !Mapping.GetCurrentKey().IsGamepadKey())
			{
				PendingMappingName = Pair.Key;
				MappingDefaultKey = Mapping.GetDefaultKey();
				break;
			}
		}
		if (!PendingMappingName.IsNone()) break;
	}

	// 기본키가 마우스인 액션은 리바인드 불가(버튼도 비활성이지만 이중 안전장치).
	if (!PendingMappingName.IsNone() && MappingDefaultKey.IsMouseButton())
	{
		PendingMappingName = NAME_None;
		return;
	}

	PendingActionAssetName = PendingMappingName.IsNone() ? NAME_None : ActionAssetName;
	PendingKeyLabelName = LabelWidgetName;
	SetText(GetPage(ERetrieveSettingsCategory::Controls), LabelWidgetName,
		FText::FromString(PendingMappingName.IsNone() ? TEXT("매핑 설정 필요") : TEXT("키를 누르세요")));

	// 다음 키 입력을 받으려면 이 위젯에 포커스가 있어야 한다.
	SetKeyboardFocus();
}

static ERetrieveAudioChannel RowKeyToAudioChannel(const FName Key)
{
	if (Key == TEXT("Music"))    return ERetrieveAudioChannel::Music;
	if (Key == TEXT("Sfx"))      return ERetrieveAudioChannel::Sfx;
	if (Key == TEXT("Ambience")) return ERetrieveAudioChannel::Ambience;
	if (Key == TEXT("UI"))       return ERetrieveAudioChannel::UI;
	if (Key == TEXT("Voice"))    return ERetrieveAudioChannel::Voice;
	return ERetrieveAudioChannel::Master;
}

static FText AudioRowLabel(const FName Key)
{
	if (Key == TEXT("Music"))    return FText::FromString(TEXT("음악"));
	if (Key == TEXT("Sfx"))      return FText::FromString(TEXT("효과음"));
	if (Key == TEXT("Ambience")) return FText::FromString(TEXT("환경음"));
	if (Key == TEXT("UI"))       return FText::FromString(TEXT("UI"));
	if (Key == TEXT("Voice"))    return FText::FromString(TEXT("음성"));
	return FText::FromString(TEXT("마스터 볼륨"));
}

static FText AudioRowDesc(const FName Key)
{
	if (Key == TEXT("Music"))    return FText::FromString(TEXT("배경 음악"));
	if (Key == TEXT("Sfx"))      return FText::FromString(TEXT("전투·UI 효과음"));
	if (Key == TEXT("Ambience")) return FText::FromString(TEXT("환경·앰비언스"));
	if (Key == TEXT("UI"))       return FText::FromString(TEXT("인터페이스 사운드"));
	if (Key == TEXT("Voice"))    return FText::FromString(TEXT("음성·보이스"));
	return FText::FromString(TEXT("전체 음량"));
}

void URetrieveSettingsPanelWidget::RefreshAudio()
{
	URetrieveSettingsSubsystem* Subsystem = GetSettingsSubsystem();
	URetrieveGameUserSettings* S = GetUserSettings();
	UUserWidget* Page = GetPage(ERetrieveSettingsCategory::Audio);
	if (!Subsystem || !S || !Page) return;

	// 구(舊) 인라인 슬라이더(아직 남아 있으면) 갱신 — 아키타입 전환 후엔 자동으로 no-op.
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

	// 슬라이더 행 아키타입(URetrieveSettingRowSlider) 값 갱신.
	if (Page->WidgetTree)
	{
		Page->WidgetTree->ForEachWidget([Subsystem](UWidget* W)
		{
			if (URetrieveSettingRowSlider* RowWidget = Cast<URetrieveSettingRowSlider>(W))
			{
				RowWidget->SetValueSilently(Subsystem->GetChannelVolume(RowKeyToAudioChannel(RowWidget->RowKey)));
			}
		});
	}

	SetChecked(Page, TEXT("Chk_MuteUnfocused"), S->bMuteWhenUnfocused);
	SetToggleByKey(Page, TEXT("Audio_MuteUnfocused"), S->bMuteWhenUnfocused);
}

void URetrieveSettingsPanelWidget::BindAudioRows()
{
	UUserWidget* Page = GetPage(ERetrieveSettingsCategory::Audio);
	if (!Page || !Page->WidgetTree)
	{
		return;
	}
	Page->WidgetTree->ForEachWidget([this](UWidget* W)
	{
		if (URetrieveSettingRowSlider* RowWidget = Cast<URetrieveSettingRowSlider>(W))
		{
			RowWidget->OnRowValueChanged.AddUniqueDynamic(this, &URetrieveSettingsPanelWidget::HandleAudioRowChanged);
			RowWidget->SetLabelTexts(AudioRowLabel(RowWidget->RowKey), AudioRowDesc(RowWidget->RowKey));
		}
	});
}

// ─── Graphics rows ───────────────────────────────────────────────────────────

static FText GraphicsRowLabel(const FName Key)
{
	if (Key == TEXT("Graphics_FrameLimit")) return FText::FromString(TEXT("프레임 제한"));
	if (Key == TEXT("Graphics_Gamma"))      return FText::FromString(TEXT("감마(밝기)"));
	return FText::GetEmpty();
}

static FText GraphicsRowDesc(const FName Key)
{
	if (Key == TEXT("Graphics_FrameLimit")) return FText::FromString(TEXT("0 = 무제한"));
	if (Key == TEXT("Graphics_Gamma"))      return FText::FromString(TEXT("화면 밝기"));
	return FText::GetEmpty();
}

void URetrieveSettingsPanelWidget::BindGraphicsRows()
{
	UUserWidget* Page = GetPage(ERetrieveSettingsCategory::Graphics);
	if (!Page || !Page->WidgetTree) return;
	Page->WidgetTree->ForEachWidget([this](UWidget* W)
	{
		if (URetrieveSettingRowSlider* Row = Cast<URetrieveSettingRowSlider>(W))
		{
			Row->SetLabelTexts(GraphicsRowLabel(Row->RowKey), GraphicsRowDesc(Row->RowKey));
			Row->OnRowValueChanged.AddUniqueDynamic(this, &URetrieveSettingsPanelWidget::HandleGraphicsRowChanged);
		}
	});
}

void URetrieveSettingsPanelWidget::HandleGraphicsRowChanged(FName RowKey, float Value)
{
	if (bRefreshingControls) return;
	if      (RowKey == TEXT("Graphics_FrameLimit")) HandleFrameLimitChanged(Value);
	else if (RowKey == TEXT("Graphics_Gamma"))      HandleGammaChanged(Value);
}

// ─── Controls rows ───────────────────────────────────────────────────────────

static FText ControlsRowLabel(const FName Key)
{
	if (Key == TEXT("Controls_MouseAll")) return FText::FromString(TEXT("마우스 감도"));
	if (Key == TEXT("Controls_MouseX"))   return FText::FromString(TEXT("ㄴ X 감도"));
	if (Key == TEXT("Controls_MouseY"))   return FText::FromString(TEXT("ㄴ Y 감도"));
	if (Key == TEXT("Controls_PadSens")) return FText::FromString(TEXT("게임패드 감도"));
	return FText::GetEmpty();
}

static FText ControlsRowDesc(const FName Key)
{
	if (Key == TEXT("Controls_MouseAll")) return FText::FromString(TEXT("통합 회전 감도"));
	if (Key == TEXT("Controls_MouseX"))   return FText::FromString(TEXT("좌우 회전 감도"));
	if (Key == TEXT("Controls_MouseY"))   return FText::FromString(TEXT("상하 회전 감도"));
	if (Key == TEXT("Controls_PadSens")) return FText::FromString(TEXT("스틱 회전 감도"));
	return FText::GetEmpty();
}

void URetrieveSettingsPanelWidget::BindControlsRows()
{
	UUserWidget* Page = GetPage(ERetrieveSettingsCategory::Controls);
	if (!Page || !Page->WidgetTree) return;
	Page->WidgetTree->ForEachWidget([this](UWidget* W)
	{
		if (URetrieveSettingRowSlider* Row = Cast<URetrieveSettingRowSlider>(W))
		{
			Row->SetLabelTexts(ControlsRowLabel(Row->RowKey), ControlsRowDesc(Row->RowKey));
			// 감도 행은 0.1..3.0 실값 범위를 쓴다(내부 슬라이더는 여전히 0..1).
			// 예전엔 Range가 없어 0..1 정규화값이 그대로 감도값으로 저장되던 버그가 있었다.
			if (Row->RowKey == TEXT("Controls_MouseAll")
				|| Row->RowKey == TEXT("Controls_MouseX") || Row->RowKey == TEXT("Controls_MouseY")
				|| Row->RowKey == TEXT("Controls_PadSens"))
			{
				Row->RangeMin = 0.1f;
				Row->RangeMax = 3.0f;
				Row->bDisplayAsPercent = false;
				Row->DisplayDecimals = 1;
			}
			Row->OnRowValueChanged.AddUniqueDynamic(this, &URetrieveSettingsPanelWidget::HandleControlsRowChanged);
		}
	});
}

void URetrieveSettingsPanelWidget::HandleControlsRowChanged(FName RowKey, float Value)
{
	if (bRefreshingControls) return;
	// Value는 Row의 Range로 이미 실값(0.1..3.0)에 매핑되어 들어온다.
	if      (RowKey == TEXT("Controls_MouseX"))   HandleMouseXChanged(Value);
	else if (RowKey == TEXT("Controls_MouseY"))   HandleMouseYChanged(Value);
	else if (RowKey == TEXT("Controls_PadSens")) HandlePadSensitivityChanged(Value);
	else if (RowKey == TEXT("Controls_MouseAll"))
	{
		// 통합 감도: X/Y를 같은 값으로 동기화(개별 조절로 달라졌어도 다시 맞춰짐).
		HandleMouseXChanged(Value);
		HandleMouseYChanged(Value);
		// 하위 X/Y 행 슬라이더 표시도 이벤트 없이 동기화.
		if (UUserWidget* Page = GetPage(ERetrieveSettingsCategory::Controls); Page && Page->WidgetTree)
		{
			Page->WidgetTree->ForEachWidget([Value](UWidget* W)
			{
				if (URetrieveSettingRowSlider* Row = Cast<URetrieveSettingRowSlider>(W))
				{
					if (Row->RowKey == TEXT("Controls_MouseX") || Row->RowKey == TEXT("Controls_MouseY"))
					{
						Row->SetRawValueSilently(Value);
					}
				}
			});
		}
	}
}

// ─── Gameplay rows ───────────────────────────────────────────────────────────

static FText GameplayRowLabel(const FName Key)
{
	if (Key == TEXT("Gameplay_SubtitleScale")) return FText::FromString(TEXT("자막 크기"));
	if (Key == TEXT("Gameplay_FOV"))           return FText::FromString(TEXT("시야각(FOV)"));
	if (Key == TEXT("Gameplay_CameraShake"))   return FText::FromString(TEXT("카메라 흔들림"));
	return FText::GetEmpty();
}

static FText GameplayRowDesc(const FName Key)
{
	if (Key == TEXT("Gameplay_SubtitleScale")) return FText::FromString(TEXT("자막 글자 크기"));
	if (Key == TEXT("Gameplay_FOV"))           return FText::FromString(TEXT("카메라 시야각"));
	if (Key == TEXT("Gameplay_CameraShake"))   return FText::FromString(TEXT("화면 흔들림 강도"));
	return FText::GetEmpty();
}

void URetrieveSettingsPanelWidget::BindGameplayRows()
{
	UUserWidget* Page = GetPage(ERetrieveSettingsCategory::Gameplay);
	if (!Page || !Page->WidgetTree) return;
	Page->WidgetTree->ForEachWidget([this](UWidget* W)
	{
		if (URetrieveSettingRowSlider* Row = Cast<URetrieveSettingRowSlider>(W))
		{
			Row->SetLabelTexts(GameplayRowLabel(Row->RowKey), GameplayRowDesc(Row->RowKey));
			// 예전엔 Range가 없어 0..1 정규화값이 그대로 FOV(도)/배율에 저장되던 버그가 있었다.
			if (Row->RowKey == TEXT("Gameplay_SubtitleScale"))
			{
				Row->RangeMin = 0.5f; Row->RangeMax = 2.0f; Row->bDisplayAsPercent = false; Row->DisplayDecimals = 0;
			}
			else if (Row->RowKey == TEXT("Gameplay_FOV"))
			{
				Row->RangeMin = 70.f; Row->RangeMax = 110.f; Row->bDisplayAsPercent = false; Row->DisplayDecimals = 0;
			}
			else if (Row->RowKey == TEXT("Gameplay_CameraShake"))
			{
				Row->RangeMin = 0.f; Row->RangeMax = 1.f; Row->bDisplayAsPercent = true;
			}
			Row->OnRowValueChanged.AddUniqueDynamic(this, &URetrieveSettingsPanelWidget::HandleGameplayRowChanged);
		}
	});
}

void URetrieveSettingsPanelWidget::HandleGameplayRowChanged(FName RowKey, float Value)
{
	if (bRefreshingControls) return;
	// Value는 Row의 Range로 이미 실값(자막 배율/FOV 도/흔들림 배율)에 매핑되어 들어온다.
	if      (RowKey == TEXT("Gameplay_SubtitleScale")) HandleSubtitleScaleChanged(Value);
	else if (RowKey == TEXT("Gameplay_FOV"))           HandleFOVChanged(Value);
	else if (RowKey == TEXT("Gameplay_CameraShake"))   HandleCameraShakeChanged(Value);
}

// ─── Accessibility rows ──────────────────────────────────────────────────────

static FText AccessibilityRowLabel(const FName Key)
{
	if (Key == TEXT("Accessibility_CBStrength")) return FText::FromString(TEXT("색맹 보정 강도"));
	if (Key == TEXT("Accessibility_UIScale"))    return FText::FromString(TEXT("UI 크기"));
	if (Key == TEXT("Accessibility_AimAssist"))  return FText::FromString(TEXT("에임 보조"));
	if (Key == TEXT("Accessibility_SubtitleBG")) return FText::FromString(TEXT("자막 배경"));
	return FText::GetEmpty();
}

static FText AccessibilityRowDesc(const FName Key)
{
	if (Key == TEXT("Accessibility_CBStrength")) return FText::FromString(TEXT("색맹 보정 세기"));
	if (Key == TEXT("Accessibility_UIScale"))    return FText::FromString(TEXT("UI 글자·요소 크기 (85~115%)"));
	if (Key == TEXT("Accessibility_AimAssist"))  return FText::FromString(TEXT("조준 보조 강도"));
	if (Key == TEXT("Accessibility_SubtitleBG")) return FText::FromString(TEXT("자막 배경 불투명도"));
	return FText::GetEmpty();
}

void URetrieveSettingsPanelWidget::BindAccessibilityRows()
{
	UUserWidget* Page = GetPage(ERetrieveSettingsCategory::Accessibility);
	if (!Page || !Page->WidgetTree) return;
	Page->WidgetTree->ForEachWidget([this](UWidget* W)
	{
		if (URetrieveSettingRowSlider* Row = Cast<URetrieveSettingRowSlider>(W))
		{
			Row->SetLabelTexts(AccessibilityRowLabel(Row->RowKey), AccessibilityRowDesc(Row->RowKey));
			if (Row->RowKey == TEXT("Accessibility_UIScale"))
			{
				// 값 박스에 슬라이더 위치(0~100)가 아니라 실제 스케일 %(85~115)가 보이도록 매핑한다.
				Row->RangeMin = 85.f;
				Row->RangeMax = 115.f;
				Row->bDisplayAsPercent = false;
				Row->DisplayDecimals = 0;
			}
			Row->OnRowValueChanged.AddUniqueDynamic(this, &URetrieveSettingsPanelWidget::HandleAccessibilityRowChanged);
		}
	});
}

void URetrieveSettingsPanelWidget::HandleAccessibilityRowChanged(FName RowKey, float Value)
{
	if (bRefreshingControls) return;
	// WBP_SettingRow_Slider는 0..1을 준다. 각 설정의 실제 범위로 매핑 후 기존 핸들러 호출.
	if      (RowKey == TEXT("Accessibility_CBStrength")) HandleColorBlindStrengthChanged(Value * 10.f);
	else if (RowKey == TEXT("Accessibility_UIScale"))    HandleUIScaleChanged(Value / 100.f); // 행 Range가 85~115(%)
	else if (RowKey == TEXT("Accessibility_AimAssist"))  HandleAimAssistChanged(Value);
	else if (RowKey == TEXT("Accessibility_SubtitleBG")) HandleSubtitleBackgroundChanged(Value);
}

// ─────────────────────────────────────────────────────────────────────────────

void URetrieveSettingsPanelWidget::HandleAudioRowChanged(FName RowKey, float Value)
{
	if (bRefreshingControls)
	{
		return;
	}
	if (URetrieveSettingsSubsystem* Subsystem = GetSettingsSubsystem())
	{
		// 행 아키타입이 값 텍스트/위치는 스스로 갱신한다. 여기선 실제 볼륨만 적용.
		Subsystem->SetChannelVolume(RowKeyToAudioChannel(RowKey), Value, true);
	}
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
	SetToggleByKey(Page, TEXT("Gameplay_Subtitles"), S->bSubtitlesEnabled);
	SetToggleByKey(Page, TEXT("Gameplay_DamageNumbers"), S->bShowDamageNumbers);
	SetToggleByKey(Page, TEXT("Gameplay_TutorialHints"), S->bTutorialHints);
	SetSlider(Page, TEXT("Sld_SubtitleScale"), S->SubtitleTextScale);
	SetSlider(Page, TEXT("Sld_FOV"), S->FieldOfView);
	SetSlider(Page, TEXT("Sld_CameraShake"), S->CameraShakeScale);
	SetText(Page, TEXT("Val_SubtitleScale"), PercentText(S->SubtitleTextScale));
	SetText(Page, TEXT("Val_FOV"), NumberText(S->FieldOfView));
	SetText(Page, TEXT("Val_CameraShake"), PercentText(S->CameraShakeScale));

	// 슬라이더 행 아키타입(URetrieveSettingRowSlider) 값 갱신. Range는 BindGameplayRows에서 이미 설정됨.
	if (Page->WidgetTree)
	{
		Page->WidgetTree->ForEachWidget([S](UWidget* W)
		{
			if (URetrieveSettingRowSlider* Row = Cast<URetrieveSettingRowSlider>(W))
			{
				if      (Row->RowKey == TEXT("Gameplay_SubtitleScale")) Row->SetRawValueSilently(S->SubtitleTextScale);
				else if (Row->RowKey == TEXT("Gameplay_FOV"))           Row->SetRawValueSilently(S->FieldOfView);
				else if (Row->RowKey == TEXT("Gameplay_CameraShake"))   Row->SetRawValueSilently(S->CameraShakeScale);
			}
		});
	}
}

void URetrieveSettingsPanelWidget::RefreshAccessibility()
{
	URetrieveGameUserSettings* S = GetUserSettings();
	UUserWidget* Page = GetPage(ERetrieveSettingsCategory::Accessibility);
	if (!S || !Page) return;
	SetText(Page, TEXT("Val_ColorBlind"), ColorBlindText(S->ColorBlindMode));
	SetText(Page, TEXT("Val_Interact"), FText::FromString(S->bHoldToInteract ? TEXT("홀드") : TEXT("토글")));
	// 접근성 슬라이더는 WBP_SettingRow_Slider(내부 USlider 0..1)로 전환됨 → RowKey로 찾아 정규화값 갱신.
	if (Page->WidgetTree)
	{
		Page->WidgetTree->ForEachWidget([&](UWidget* W)
		{
			URetrieveSettingRowSlider* Row = Cast<URetrieveSettingRowSlider>(W);
			if (!Row) return;
			if      (Row->RowKey == TEXT("Accessibility_CBStrength")) Row->SetValueSilently(S->ColorBlindStrength / 10.f);
			else if (Row->RowKey == TEXT("Accessibility_UIScale"))    Row->SetRawValueSilently(S->UITextScale * 100.f);
			else if (Row->RowKey == TEXT("Accessibility_AimAssist"))  Row->SetValueSilently(S->AimAssistStrength);
			else if (Row->RowKey == TEXT("Accessibility_SubtitleBG")) Row->SetValueSilently(S->SubtitleBackgroundOpacity);
		});
	}
	SetChecked(Page, TEXT("Chk_HighContrast"), S->bHighContrastHUD);
	SetChecked(Page, TEXT("Chk_ReduceMotion"), S->bReduceMotion);
	SetToggleByKey(Page, TEXT("Accessibility_HighContrast"), S->bHighContrastHUD);
	SetToggleByKey(Page, TEXT("Accessibility_ReduceMotion"), S->bReduceMotion);
}

void URetrieveSettingsPanelWidget::ApplyRuntimeStyle()
{
	// 색 스타일은 고대비 테마일 때만 코드로 덮어쓴다. 기본 테마에선 WBP 디자인을 그대로 둔다.
	// (슬라이더 값 범위 등 '기능' 설정은 아래에서 테마와 무관하게 항상 적용한다.)
	const bool bHighContrast = URetrieveUISettingsLibrary::IsHighContrastEnabled();
	const URetrieveUITheme* Theme = URetrieveUISettingsLibrary::GetActiveUITheme();
	const FLinearColor BarColor = Theme ? Theme->PanelBorder : FLinearColor(0.30f, 0.24f, 0.12f, 1.f);
	const FLinearColor HandleColor = Theme ? Theme->SliderHandle : FLinearColor(0.90f, 0.74f, 0.38f, 1.f);
	// 색 덮어쓰기(슬라이더 바/핸들)는 고대비 테마에서만. 기본 테마에선 WBP 디자인을 유지한다.
	if (bHighContrast)
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
		// (과거) 화살표 버튼 배경을 DarkChip으로 덮던 코드는 제거했다.
		// 하드코딩 목록이라 일부 버튼만 검게 칠해 비일관적이었고, 고대비 가독성은 이제
		// ApplyHighContrastToAllWidgets의 전역 텍스트 그림자가 담당한다(버튼은 WBP 디자인 유지).
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
	// 접근성 CBStrength/UIScale는 WBP_SettingRow_Slider(0..1)로 전환 → 범위 매핑은 HandleAccessibilityRowChanged에서 처리.
}

void URetrieveSettingsPanelWidget::ApplyOptionAvailability()
{
	// 소비 구현이 없는 옵션은 행(Row_*) 전체를 Collapsed로 숨긴다.
	// 위젯을 찾지 못하면 안전하게 무시된다(에셋 변경 없이 런타임 처리).
	for (const RetrieveSettingAvailability::FOptionRow& Row : RetrieveSettingAvailability::GetOptionRows())
	{
		if (Row.bAvailable)
		{
			continue;
		}
		if (UWidget* RowWidget = FindWidget<UWidget>(GetPage(Row.Category), Row.RowWidgetName))
		{
			RowWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// 섹션의 모든 행이 숨겨졌으면 헤더/구분선도 숨겨 빈 제목(예: "입력")이 남지 않게 한다.
	for (const RetrieveSettingAvailability::FOptionSection& Sec : RetrieveSettingAvailability::GetOptionSections())
	{
		UUserWidget* Page = GetPage(Sec.Category);
		if (!Page) continue;
		bool bAnyVisible = false;
		for (const TCHAR* RowName : Sec.RowWidgetNames)
		{
			UWidget* RowWidget = FindWidget<UWidget>(Page, RowName);
			if (RowWidget && RowWidget->GetVisibility() != ESlateVisibility::Collapsed)
			{
				bAnyVisible = true;
				break;
			}
		}
		if (bAnyVisible) continue;
		if (UWidget* Hdr = FindWidget<UWidget>(Page, Sec.HeaderWidgetName))
		{
			Hdr->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (Sec.DividerWidgetName)
		{
			if (UWidget* Div = FindWidget<UWidget>(Page, Sec.DividerWidgetName))
			{
				Div->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
}

void URetrieveSettingsPanelWidget::HandleGraphicsTab() { SelectCategory(ERetrieveSettingsCategory::Graphics); }
void URetrieveSettingsPanelWidget::HandleControlsTab() { SelectCategory(ERetrieveSettingsCategory::Controls); }
void URetrieveSettingsPanelWidget::HandleAudioTab() { SelectCategory(ERetrieveSettingsCategory::Audio); }
void URetrieveSettingsPanelWidget::HandleGameplayTab() { SelectCategory(ERetrieveSettingsCategory::Gameplay); }
void URetrieveSettingsPanelWidget::HandleAccessibilityTab() { SelectCategory(ERetrieveSettingsCategory::Accessibility); }
void URetrieveSettingsPanelWidget::HandleApply() { ApplyAndSave(); }
void URetrieveSettingsPanelWidget::HandleReset() { ResetCurrentCategory(); }
void URetrieveSettingsPanelWidget::HandleClose() { RequestClose(); }

void URetrieveSettingsPanelWidget::HandleRespawn()
{
	const ARetrieveGameState* GS = GetWorld() ? GetWorld()->GetGameState<ARetrieveGameState>() : nullptr;
	if (!GS || GS->GetSessionState() != ERetrieveSessionState::InGame)
	{
		return;
	}

	if (ARetrievePlayerController* PC = Cast<ARetrievePlayerController>(GetOwningPlayer()))
	{
		PC->RequestUnstuck();
	}
	RequestClose();
}

void URetrieveSettingsPanelWidget::HandleSettingChanged(ERetrieveSettingsCategory Category)
{
	// 고대비 등 접근성 변경(또는 전체 적용) 시 화면 색상을 즉시 다시 적용한다.
	if (Category == ERetrieveSettingsCategory::Accessibility || Category == ERetrieveSettingsCategory::MAX)
	{
		ApplyRuntimeStyle();
		UpdateScreenForCategory(CurrentCategory);
	}
}

#define APPLY_PREVIEW(Category) if (URetrieveSettingsSubsystem* Subsystem = GetSettingsSubsystem()) Subsystem->ApplyCategory(Category, false)
#define DEFINE_QUALITY_HANDLER(Func, Getter, Setter, Delta, ValueName) \
	void URetrieveSettingsPanelWidget::Func() { if (URetrieveGameUserSettings* S = GetUserSettings()) { \
	const int32 V = WrapIndex(S->Getter() + Delta, 4); S->Setter(V); \
	APPLY_PREVIEW(ERetrieveSettingsCategory::Graphics); RefreshCurrentPage(); } }

void URetrieveSettingsPanelWidget::HandleWindowModePrev() { if (URetrieveGameUserSettings* S = GetUserSettings()) { const auto V = static_cast<ERetrieveWindowMode>(WrapIndex(static_cast<int32>(S->GetRetrieveWindowMode()) - 1, 3)); S->SetRetrieveWindowMode(V); APPLY_PREVIEW(ERetrieveSettingsCategory::Graphics); RefreshCurrentPage(); /* 해상도 표시가 창 모드에 따라 달라지므로 함께 갱신 */ } }
void URetrieveSettingsPanelWidget::HandleWindowModeNext() { if (URetrieveGameUserSettings* S = GetUserSettings()) { const auto V = static_cast<ERetrieveWindowMode>(WrapIndex(static_cast<int32>(S->GetRetrieveWindowMode()) + 1, 3)); S->SetRetrieveWindowMode(V); APPLY_PREVIEW(ERetrieveSettingsCategory::Graphics); RefreshCurrentPage(); /* 해상도 표시가 창 모드에 따라 달라지므로 함께 갱신 */ } }

namespace
{
	// 지원 해상도: RHI가 보고하는 모니터 실측 전체화면 해상도(첫 사용 시 1회 구축).
	// 목록을 얻지 못하면 흔한 16:9 목록으로 폴백한다. 하드코딩 목록은 모니터가 지원하지
	// 않는 해상도를 제안해 "이상한 해상도로 설정되는" 원인이었다.
	const TArray<FIntPoint>& GetSupportedResolutionList()
	{
		static TArray<FIntPoint> Resolutions;
		if (Resolutions.Num() == 0)
		{
			TArray<FIntPoint> FromRHI;
			if (UKismetSystemLibrary::GetSupportedFullscreenResolutions(FromRHI))
			{
				for (const FIntPoint& R : FromRHI)
				{
					// 너무 작은 해상도는 UI가 깨져 제외한다.
					if (R.X >= 1280 && R.Y >= 720)
					{
						Resolutions.AddUnique(R);
					}
				}
			}
			if (Resolutions.Num() == 0)
			{
				Resolutions = { {1280, 720}, {1920, 1080}, {2560, 1440}, {3840, 2160} };
			}
			Resolutions.Sort([](const FIntPoint& A, const FIntPoint& B)
			{
				return A.X != B.X ? A.X < B.X : A.Y < B.Y;
			});
		}
		return Resolutions;
	}

	int32 FindCurrentResolutionIndex(const FIntPoint& Current)
	{
		const TArray<FIntPoint>& List = GetSupportedResolutionList();
		// 정확히 일치하는 항목이 없으면(OS 배율 창 등) 가장 가까운 해상도로 스냅한다.
		// 예전에는 무조건 1920x1080으로 간주해 한 칸 이동이 엉뚱한 해상도로 점프했다.
		int32 BestIndex = 0;
		int64 BestDist = MAX_int64;
		for (int32 i = 0; i < List.Num(); ++i)
		{
			if (List[i] == Current)
			{
				return i;
			}
			const int64 Dx = List[i].X - Current.X;
			const int64 Dy = List[i].Y - Current.Y;
			const int64 Dist = Dx * Dx + Dy * Dy;
			if (Dist < BestDist)
			{
				BestDist = Dist;
				BestIndex = i;
			}
		}
		return BestIndex;
	}

	FText ResolutionText(const FIntPoint& Res)
	{
		return FText::FromString(FString::Printf(TEXT("%d x %d"), Res.X, Res.Y));
	}
}

void URetrieveSettingsPanelWidget::HandleResolutionPrev()
{
	if (URetrieveGameUserSettings* S = GetUserSettings())
	{
		// 테두리 없는 창은 항상 데스크톱 해상도를 쓰므로 선택이 무의미하다.
		if (S->GetRetrieveWindowMode() == ERetrieveWindowMode::WindowedFullscreen)
		{
			return;
		}
		const TArray<FIntPoint>& List = GetSupportedResolutionList();
		const int32 Idx = WrapIndex(FindCurrentResolutionIndex(S->GetScreenResolution()) - 1, List.Num());
		S->SetScreenResolution(List[Idx]);
		// Apply는 하지 않는다 — 즉시 적용 시 창 크기가 바뀌어 UI가 잘린다. Apply 버튼 클릭 시 적용된다.
		SetText(GetPage(ERetrieveSettingsCategory::Graphics), TEXT("Val_Resolution"), ResolutionText(List[Idx]));
	}
}

void URetrieveSettingsPanelWidget::HandleResolutionNext()
{
	if (URetrieveGameUserSettings* S = GetUserSettings())
	{
		if (S->GetRetrieveWindowMode() == ERetrieveWindowMode::WindowedFullscreen)
		{
			return;
		}
		const TArray<FIntPoint>& List = GetSupportedResolutionList();
		const int32 Idx = WrapIndex(FindCurrentResolutionIndex(S->GetScreenResolution()) + 1, List.Num());
		S->SetScreenResolution(List[Idx]);
		// Apply는 하지 않는다 — 즉시 적용 시 창 크기가 바뀌어 UI가 잘린다. Apply 버튼 클릭 시 적용된다.
		SetText(GetPage(ERetrieveSettingsCategory::Graphics), TEXT("Val_Resolution"), ResolutionText(List[Idx]));
	}
}
namespace
{
	// 전체 품질이 "사용자 지정"(-1, 하위 품질 혼합)일 때는 노출된 하위 품질의 평균을
	// 현재 단계로 간주하고 스텝한다. SetOverallScalabilityLevel이 하위 품질을 모두 맞춘다.
	int32 EffectiveOverallQuality(const URetrieveGameUserSettings* S)
	{
		const int32 Overall = S->GetOverallScalabilityLevel();
		if (Overall >= 0)
		{
			return Overall;
		}
		const float Avg = (S->GetShadowQuality() + S->GetTextureQuality() + S->GetVisualEffectQuality()) / 3.f;
		return FMath::Clamp(FMath::RoundToInt(Avg), 0, 3);
	}
}

void URetrieveSettingsPanelWidget::HandleQualityPrev()
{
	if (URetrieveGameUserSettings* S = GetUserSettings())
	{
		S->SetOverallScalabilityLevel(WrapIndex(EffectiveOverallQuality(S) - 1, 4));
		APPLY_PREVIEW(ERetrieveSettingsCategory::Graphics);
		RefreshCurrentPage();
	}
}

void URetrieveSettingsPanelWidget::HandleQualityNext()
{
	if (URetrieveGameUserSettings* S = GetUserSettings())
	{
		S->SetOverallScalabilityLevel(WrapIndex(EffectiveOverallQuality(S) + 1, 4));
		APPLY_PREVIEW(ERetrieveSettingsCategory::Graphics);
		RefreshCurrentPage();
	}
}

DEFINE_QUALITY_HANDLER(HandleShadowPrev, GetShadowQuality, SetShadowQuality, -1, "Val_Shadow")
DEFINE_QUALITY_HANDLER(HandleShadowNext, GetShadowQuality, SetShadowQuality, 1, "Val_Shadow")
DEFINE_QUALITY_HANDLER(HandleTexturePrev, GetTextureQuality, SetTextureQuality, -1, "Val_Texture")
DEFINE_QUALITY_HANDLER(HandleTextureNext, GetTextureQuality, SetTextureQuality, 1, "Val_Texture")
DEFINE_QUALITY_HANDLER(HandleEffectsPrev, GetVisualEffectQuality, SetVisualEffectQuality, -1, "Val_Effects")
DEFINE_QUALITY_HANDLER(HandleEffectsNext, GetVisualEffectQuality, SetVisualEffectQuality, 1, "Val_Effects")
#undef DEFINE_QUALITY_HANDLER

void URetrieveSettingsPanelWidget::HandleVSyncChanged(bool b) { if (bRefreshingControls) return; if (auto* S = GetUserSettings()) { S->SetVSyncEnabled(b); APPLY_PREVIEW(ERetrieveSettingsCategory::Graphics); } }
void URetrieveSettingsPanelWidget::HandleMotionBlurChanged(bool b) { if (bRefreshingControls) return; if (auto* S = GetUserSettings()) { S->bMotionBlur = b; APPLY_PREVIEW(ERetrieveSettingsCategory::Graphics); } }
void URetrieveSettingsPanelWidget::HandleFrameLimitChanged(float V) { if (bRefreshingControls) return; if (auto* S = GetUserSettings()) { const float FPS = FMath::Lerp(30.f, 240.f, V); S->SetFrameRateLimit(FPS); SetText(GetPage(ERetrieveSettingsCategory::Graphics), TEXT("Val_FrameLimit"), NumberText(FPS)); } }
void URetrieveSettingsPanelWidget::HandleGammaChanged(float V) { if (bRefreshingControls) return; if (auto* S = GetUserSettings()) { const float Gamma = FMath::Lerp(1.8f, 2.6f, V); S->GammaLevel = Gamma; SetText(GetPage(ERetrieveSettingsCategory::Graphics), TEXT("Val_Gamma"), NumberText(Gamma, 1)); APPLY_PREVIEW(ERetrieveSettingsCategory::Graphics); } }

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
void URetrieveSettingsPanelWidget::HandleBowAimModeChanged(bool bToggleMode) { if (bRefreshingControls) return; if (auto* S = GetUserSettings()) { S->bBowAimToggleMode = bToggleMode; APPLY_PREVIEW(ERetrieveSettingsCategory::Controls); } }
#define DEFINE_REBIND_HANDLER(Func, ActionAssetName, KeyTextWidgetName) \
	void URetrieveSettingsPanelWidget::Func() { BeginRebind(TEXT(ActionAssetName), TEXT(KeyTextWidgetName)); }
DEFINE_REBIND_HANDLER(HandleRebindDodge, "IA_Roll", "Key_Dodge")
DEFINE_REBIND_HANDLER(HandleRebindLockOn, "IA_LockOn", "Key_LockOn")
DEFINE_REBIND_HANDLER(HandleRebindSprint, "IA_Sprint", "Key_Sprint")
DEFINE_REBIND_HANDLER(HandleRebindJump, "IA_Jump", "Key_Jump")
DEFINE_REBIND_HANDLER(HandleRebindInteract, "IA_Interaction", "Key_Interaction")
DEFINE_REBIND_HANDLER(HandleRebindHeavyAttack, "IA_HeavyAttack", "Key_HeavyAttack")
DEFINE_REBIND_HANDLER(HandleRebindGuard, "IA_Guard", "Key_Guard")
DEFINE_REBIND_HANDLER(HandleRebindCrouch, "IA_Crouch", "Key_Crouch")
DEFINE_REBIND_HANDLER(HandleRebindBurst, "IA_Burst", "Key_Burst")
DEFINE_REBIND_HANDLER(HandleRebindAbsorb, "IA_Absorb", "Key_Absorb")
DEFINE_REBIND_HANDLER(HandleRebindRecallLumen, "IA_RecallLumen", "Key_RecallLumen")
DEFINE_REBIND_HANDLER(HandleRebindElement1, "IA_ElementMode1", "Key_Element1")
DEFINE_REBIND_HANDLER(HandleRebindElement2, "IA_ElementMode2", "Key_Element2")
DEFINE_REBIND_HANDLER(HandleRebindElement3, "IA_ElementMode3", "Key_Element3")
#undef DEFINE_REBIND_HANDLER

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
void URetrieveSettingsPanelWidget::HandleUIScaleChanged(float V) { if (bRefreshingControls) return; if (auto* S = GetUserSettings()) { S->UITextScale = V; SetText(GetPage(ERetrieveSettingsCategory::Accessibility), TEXT("Val_UIScale"), PercentText(V)); /* UI 크기는 Apply 시에만 적용(드래그 중 화면 흔들림 방지). 여기선 값/라벨만 갱신. */ } }
void URetrieveSettingsPanelWidget::HandleAimAssistChanged(float V) { if (bRefreshingControls) return; if (auto* S = GetUserSettings()) { S->AimAssistStrength = V; SetText(GetPage(ERetrieveSettingsCategory::Accessibility), TEXT("Val_AimAssist"), PercentText(V)); APPLY_PREVIEW(ERetrieveSettingsCategory::Accessibility); } }
void URetrieveSettingsPanelWidget::HandleSubtitleBackgroundChanged(float V) { if (bRefreshingControls) return; if (auto* S = GetUserSettings()) { S->SubtitleBackgroundOpacity = V; SetText(GetPage(ERetrieveSettingsCategory::Accessibility), TEXT("Val_SubtitleBG"), PercentText(V)); APPLY_PREVIEW(ERetrieveSettingsCategory::Accessibility); } }
void URetrieveSettingsPanelWidget::HandleHighContrastChanged(bool b) { if (bRefreshingControls) return; if (auto* S = GetUserSettings()) { S->bHighContrastHUD = b; APPLY_PREVIEW(ERetrieveSettingsCategory::Accessibility); } }
void URetrieveSettingsPanelWidget::HandleReduceMotionChanged(bool b) { if (bRefreshingControls) return; if (auto* S = GetUserSettings()) { S->bReduceMotion = b; APPLY_PREVIEW(ERetrieveSettingsCategory::Accessibility); } }

// ── Toggle Row 핸들러 ─────────────────────────────────────────────────────────

void URetrieveSettingsPanelWidget::HandleGraphicsToggleChanged(FName RowKey, bool bValue)
{
	if (bRefreshingControls) return;
	if (RowKey == TEXT("Graphics_VSync"))      HandleVSyncChanged(bValue);
	else if (RowKey == TEXT("Graphics_MotionBlur")) HandleMotionBlurChanged(bValue);
}

void URetrieveSettingsPanelWidget::HandleControlsToggleChanged(FName RowKey, bool bValue)
{
	if (bRefreshingControls) return;
	if (RowKey == TEXT("Controls_InvertY"))   HandleInvertYChanged(bValue);
	else if (RowKey == TEXT("Controls_Vibration")) HandleVibrationChanged(bValue);
	else if (RowKey == TEXT("Controls_BowAimToggle")) HandleBowAimModeChanged(bValue);
}

void URetrieveSettingsPanelWidget::HandleAudioToggleChanged(FName RowKey, bool bValue)
{
	if (bRefreshingControls) return;
	if (RowKey == TEXT("Audio_MuteUnfocused")) HandleMuteUnfocusedChanged(bValue);
}

void URetrieveSettingsPanelWidget::HandleGameplayToggleChanged(FName RowKey, bool bValue)
{
	if (bRefreshingControls) return;
	if      (RowKey == TEXT("Gameplay_Subtitles"))      HandleSubtitlesChanged(bValue);
	else if (RowKey == TEXT("Gameplay_DamageNumbers"))  HandleDamageNumbersChanged(bValue);
	else if (RowKey == TEXT("Gameplay_TutorialHints"))  HandleTutorialHintsChanged(bValue);
}

void URetrieveSettingsPanelWidget::HandleAccessibilityToggleChanged(FName RowKey, bool bValue)
{
	if (bRefreshingControls) return;
	if      (RowKey == TEXT("Accessibility_HighContrast")) HandleHighContrastChanged(bValue);
	else if (RowKey == TEXT("Accessibility_ReduceMotion")) HandleReduceMotionChanged(bValue);
}

#undef DEFINE_FLOAT_SETTING_HANDLER
#undef APPLY_PREVIEW
