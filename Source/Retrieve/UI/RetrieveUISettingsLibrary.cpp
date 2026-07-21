#include "UI/RetrieveUISettingsLibrary.h"
#include "UI/RetrieveUITheme.h"
#include "Settings/RetrieveSettingsConfig.h"
#include "Settings/RetrieveGameUserSettings.h"
#include "Settings/RetrieveSettingsSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "UserSettings/EnhancedInputUserSettings.h"
#include "UI/RetrieveSettingsPanelWidget.h"
#include "Engine/LocalPlayer.h"

URetrieveUITheme* URetrieveUISettingsLibrary::GetActiveUITheme()
{
	const URetrieveSettingsConfig* Cfg = GetDefault<URetrieveSettingsConfig>();
	const URetrieveGameUserSettings* S = URetrieveGameUserSettings::Get();
	if (!Cfg)
	{
		return nullptr;
	}

	const bool bHighContrast = S && S->bHighContrastHUD;
	const TSoftObjectPtr<URetrieveUITheme>& Selected =
		(bHighContrast && !Cfg->HighContrastUITheme.IsNull()) ? Cfg->HighContrastUITheme : Cfg->DefaultUITheme;

	return Selected.LoadSynchronous();
}

float URetrieveUISettingsLibrary::GetUIScale()
{
	const URetrieveGameUserSettings* S = URetrieveGameUserSettings::Get();
	return S ? FMath::Clamp(S->UITextScale, 0.5f, 2.f) : 1.f;
}

bool URetrieveUISettingsLibrary::IsHighContrastEnabled()
{
	const URetrieveGameUserSettings* S = URetrieveGameUserSettings::Get();
	return S ? S->bHighContrastHUD : false;
}

bool URetrieveUISettingsLibrary::IsReduceMotionEnabled()
{
	const URetrieveGameUserSettings* S = URetrieveGameUserSettings::Get();
	return S ? S->bReduceMotion : false;
}

bool URetrieveUISettingsLibrary::IsHUDHidden(const UObject* WorldContextObject)
{
	// 확정값(Apply/Reset/취소로 확정) 우선 — 프리뷰 토글만으로는 숨기지 않는다.
	if (const URetrieveSettingsSubsystem* Subsystem = URetrieveSettingsSubsystem::Get(WorldContextObject))
	{
		return Subsystem->IsHideHUDApplied();
	}
	// 서브시스템 미확보 시 저장값 폴백.
	const URetrieveGameUserSettings* S = URetrieveGameUserSettings::Get();
	return S && S->bHideHUD;
}

void URetrieveUISettingsLibrary::StopAnimationsRecursive(UUserWidget* Root)
{
	if (!Root)
	{
		return;
	}
	Root->StopAllAnimations();
	if (!Root->WidgetTree)
	{
		return;
	}
	Root->WidgetTree->ForEachWidget([](UWidget* W)
	{
		if (UUserWidget* Nested = Cast<UUserWidget>(W))
		{
			URetrieveUISettingsLibrary::StopAnimationsRecursive(Nested);
		}
	});
}

namespace
{
	/** 고대비 적용 전 텍스트의 원래 그림자 상태(끌 때 복원용). */
	struct FHighContrastOriginalShadow
	{
		FVector2D Offset = FVector2D::ZeroVector;
		FLinearColor Color = FLinearColor::Transparent;
	};
	TMap<TWeakObjectPtr<UTextBlock>, FHighContrastOriginalShadow> GHighContrastOriginals;

	/** 중첩 UserWidget까지 재귀하며 모든 TextBlock에 고대비 그림자를 넣는다(원본은 최초 1회 기록). */
	void ApplyHighContrastShadowRecursive(UUserWidget* Root)
	{
		if (!Root || !Root->WidgetTree)
		{
			return;
		}
		Root->WidgetTree->ForEachWidget([](UWidget* W)
		{
			if (UTextBlock* Text = Cast<UTextBlock>(W))
			{
				if (!GHighContrastOriginals.Contains(Text))
				{
					FHighContrastOriginalShadow Original;
					Original.Offset = Text->GetShadowOffset();
					Original.Color = Text->GetShadowColorAndOpacity();
					GHighContrastOriginals.Add(Text, Original);
				}
				Text->SetShadowOffset(FVector2D(1.5f, 1.5f));
				Text->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.9f));
			}
			else if (UUserWidget* Nested = Cast<UUserWidget>(W))
			{
				ApplyHighContrastShadowRecursive(Nested);
			}
		});
	}
}

void URetrieveUISettingsLibrary::ApplyHighContrastToAllWidgets(UObject* WorldContextObject)
{
	if (IsHighContrastEnabled())
	{
		if (!WorldContextObject)
		{
			return;
		}
		TArray<UUserWidget*> TopLevelWidgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
			WorldContextObject, TopLevelWidgets, UUserWidget::StaticClass(), /*TopLevelOnly*/ true);
		for (UUserWidget* Widget : TopLevelWidgets)
		{
			ApplyHighContrastShadowRecursive(Widget);
		}
		return;
	}

	// 끔: 기록해 둔 원본 그림자를 전부 복원한다(열거에 의존하지 않아 이미 닫힌 위젯도 안전).
	for (TPair<TWeakObjectPtr<UTextBlock>, FHighContrastOriginalShadow>& Pair : GHighContrastOriginals)
	{
		if (UTextBlock* Text = Pair.Key.Get())
		{
			Text->SetShadowOffset(Pair.Value.Offset);
			Text->SetShadowColorAndOpacity(Pair.Value.Color);
		}
	}
	GHighContrastOriginals.Empty();
}

void URetrieveUISettingsLibrary::ApplyHighContrastToTree(UUserWidget* Root)
{
	if (IsHighContrastEnabled())
	{
		ApplyHighContrastShadowRecursive(Root);
	}
}

namespace
{
	/** 다이어그램에서 "관리 대상"(리바인드 시 색+라벨을 옮겨 그릴 수 있는) 물리 키 1개. */
	struct FControlsGuideKeySlot
	{
		FKey PhysicalKey;
		FName BorderWidgetName;   // 키캡 배경색(Border)
		FName KeyTextWidgetName;  // 키 이름 텍스트(예: "Z") — 항상 표시, 색만 갱신
		FName LabelWidgetName;    // 액션 이름 텍스트(예: "타겟 고정") — 빈 문자열이면 숨김
	};

	/** 관리 대상 액션 1개. 원래 기본 위치와 무관하게, 현재 바인딩된 물리 키를 찾아 색칠한다. */
	struct FControlsGuideAction
	{
		FName ActionAssetName;
		const TCHAR* DisplayLabel;
		FLinearColor CategoryColor;
	};

	// 관리 대상 물리 키 25개. 원래 라벨이 있던 12자리(Tab/Q/E/R/F/L/Ctrl/Space/1/2/3/Shift) +
	// 홈로우 근처 실용 키 13자리(G/H/J/K/Z/X/C/V/B/Y/U/O/P, WBP_ControlsGuide 편집으로 라벨 슬롯 추가함).
	// 위젯명은 Monolith 스펙 자동 생성명(nXXX) 또는 새로 만든 이름(KeyTxt_*, Lbl_*)이다.
	// 설정 창 리바인드 허용 키 검증(IsControlsGuideDisplayableKey)과 안내 갱신이 공유하는 단일 원본.
	TArrayView<const FControlsGuideKeySlot> GetControlsGuideKeySlots()
	{
		static const FControlsGuideKeySlot KeySlots[] = {
			{ EKeys::Tab,         TEXT("n101"), TEXT("n98"),  TEXT("n99") },
			{ EKeys::Q,           TEXT("n106"), TEXT("n103"), TEXT("n104") },
			{ EKeys::E,           TEXT("n116"), TEXT("n113"), TEXT("n114") },
			{ EKeys::R,           TEXT("n121"), TEXT("n118"), TEXT("n119") },
			{ EKeys::F,           TEXT("n176"), TEXT("n173"), TEXT("n174") },
			{ EKeys::L,           TEXT("n193"), TEXT("n190"), TEXT("n191") },
			{ EKeys::LeftControl, TEXT("n251"), TEXT("n248"), TEXT("n249") },
			{ EKeys::SpaceBar,    TEXT("n262"), TEXT("n259"), TEXT("n260") },
			{ EKeys::One,         TEXT("n55"),  TEXT("n52"),  TEXT("n53") },
			{ EKeys::Two,         TEXT("n60"),  TEXT("n57"),  TEXT("n58") },
			{ EKeys::Three,       TEXT("n65"),  TEXT("n62"),  TEXT("n63") },
			{ EKeys::LeftShift,   TEXT("n208"), TEXT("n205"), TEXT("n206") },
			{ EKeys::G, TEXT("n179"), TEXT("KeyTxt_G"), TEXT("Lbl_G") },
			{ EKeys::H, TEXT("n182"), TEXT("KeyTxt_H"), TEXT("Lbl_H") },
			{ EKeys::J, TEXT("n185"), TEXT("KeyTxt_J"), TEXT("Lbl_J") },
			{ EKeys::K, TEXT("n188"), TEXT("KeyTxt_K"), TEXT("Lbl_K") },
			{ EKeys::Z, TEXT("n211"), TEXT("KeyTxt_Z"), TEXT("Lbl_Z") },
			{ EKeys::X, TEXT("n214"), TEXT("KeyTxt_X"), TEXT("Lbl_X") },
			{ EKeys::C, TEXT("n217"), TEXT("KeyTxt_C"), TEXT("Lbl_C") },
			{ EKeys::V, TEXT("n220"), TEXT("KeyTxt_V"), TEXT("Lbl_V") },
			{ EKeys::B, TEXT("n223"), TEXT("KeyTxt_B"), TEXT("Lbl_B") },
			{ EKeys::Y, TEXT("n129"), TEXT("KeyTxt_Y"), TEXT("Lbl_Y") },
			{ EKeys::U, TEXT("n132"), TEXT("KeyTxt_U"), TEXT("Lbl_U") },
			{ EKeys::O, TEXT("n140"), TEXT("KeyTxt_O"), TEXT("Lbl_O") },
			{ EKeys::P, TEXT("n143"), TEXT("KeyTxt_P"), TEXT("Lbl_P") },
		};
		return KeySlots;
	}

	/**
	 * 게임 시스템이 하드코딩으로 점유해 EnhancedInput 중복 검사에 잡히지 않는 키.
	 * 다이어그램에 그려져 있어도 리바인드 대상에서 항상 제외한다.
	 */
	bool IsSystemReservedKey(const FKey& Key)
	{
		static const TSet<FKey> Reserved = {
			EKeys::W, EKeys::A, EKeys::S, EKeys::D, // 이동
			EKeys::T,                               // 월드맵 마커 배치(WorldMapWidget 하드코딩)
			EKeys::N,                               // 미니맵 회전
			EKeys::I,                               // 인벤토리
			EKeys::M,                               // 월드맵
			EKeys::K,                               // 스킬 안내(컨트롤러 하드코딩)
			EKeys::Four, EKeys::Five,               // 아이템 사용
			EKeys::Escape,                          // 일시정지/패널 닫기
			EKeys::F10,                             // 설정
		};
		return Reserved.Contains(Key);
	}

	/** 키캡 텍스트("Q", "Tab", ";", "F1" 등)를 물리 키로 해석한다. 모르는 텍스트면 무효 키. */
	FKey KeyFromCapText(const FString& RawText)
	{
		const FString Text = RawText.TrimStartAndEnd();
		if (Text.Len() == 1)
		{
			const TCHAR C = FChar::ToUpper(Text[0]);
			if (C >= TEXT('A') && C <= TEXT('Z'))
			{
				return FKey(FName(FString::Chr(C)));
			}
			static const FKey Digits[] = { EKeys::Zero, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four,
				EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
			if (C >= TEXT('0') && C <= TEXT('9'))
			{
				return Digits[C - TEXT('0')];
			}
			switch (C)
			{
			case TEXT('`'): case TEXT('~'): return EKeys::Tilde;
			case TEXT(';'):  return EKeys::Semicolon;
			case TEXT('\''): return EKeys::Apostrophe;
			case TEXT(','):  return EKeys::Comma;
			case TEXT('.'):  return EKeys::Period;
			case TEXT('/'):  return EKeys::Slash;
			case TEXT('\\'): return EKeys::Backslash;
			case TEXT('['):  return EKeys::LeftBracket;
			case TEXT(']'):  return EKeys::RightBracket;
			case TEXT('-'):  return EKeys::Hyphen;
			case TEXT('='):  return EKeys::Equals;
			default:         return FKey();
			}
		}

		const FString Upper = Text.ToUpper().Replace(TEXT(" "), TEXT(""));
		static const TMap<FString, FKey> Named = {
			{ TEXT("TAB"),       EKeys::Tab },
			{ TEXT("SHIFT"),     EKeys::LeftShift },
			{ TEXT("CTRL"),      EKeys::LeftControl },
			{ TEXT("CONTROL"),   EKeys::LeftControl },
			{ TEXT("ALT"),       EKeys::LeftAlt },
			{ TEXT("SPACE"),     EKeys::SpaceBar },
			{ TEXT("SPACEBAR"),  EKeys::SpaceBar },
			{ TEXT("CAPS"),      EKeys::CapsLock },
			{ TEXT("CAPSLOCK"),  EKeys::CapsLock },
			{ TEXT("ENTER"),     EKeys::Enter },
			{ TEXT("RETURN"),    EKeys::Enter },
			{ TEXT("BACK"),      EKeys::BackSpace },
			{ TEXT("BACKSPACE"), EKeys::BackSpace },
			{ TEXT("F1"),  EKeys::F1 },  { TEXT("F2"),  EKeys::F2 },  { TEXT("F3"),  EKeys::F3 },
			{ TEXT("F4"),  EKeys::F4 },  { TEXT("F5"),  EKeys::F5 },  { TEXT("F6"),  EKeys::F6 },
			{ TEXT("F7"),  EKeys::F7 },  { TEXT("F8"),  EKeys::F8 },  { TEXT("F9"),  EKeys::F9 },
			{ TEXT("F11"), EKeys::F11 }, { TEXT("F12"), EKeys::F12 },
		};
		const FKey* Found = Named.Find(Upper);
		return Found ? *Found : FKey();
	}

	/** 다이어그램에서 발견한 관리 가능한 키캡 1개. Label은 없을 수 있다(리바인드가 오면 동적 생성). */
	struct FDiscoveredKeycap
	{
		FKey Key;
		UBorder* Border = nullptr;
		UTextBlock* KeyText = nullptr;
		UTextBlock* Label = nullptr;
	};

	/** 동적 생성 라벨의 위젯 이름 접두사. 재갱신 시 "라벨이 채워진 키캡" 제외 규칙에서 우리 라벨을 구분. */
	const TCHAR* DynamicLabelPrefix = TEXT("DynLbl_");

	/**
	 * 키보드 다이어그램의 키캡(Border > [VerticalBox >] 키이름 TextBlock[, 라벨 TextBlock])을 전부 찾는다.
	 * 1) 알려진 기본 슬롯은 정확한 위젯 이름으로 등록한다.
	 * 2) 나머지는 키 이름 텍스트 매칭으로 발견하되, 라벨 문구가 이미 박힌 키캡(WASD 이동, 4/5 아이템 등
	 *    하드코딩 기능 표기)과 시스템 예약 키는 제외한다 — 리셋으로 그 문구가 지워지는 사고 방지.
	 */
	void DiscoverKeycaps(UUserWidget* Guide, TArray<FDiscoveredKeycap>& OutSlots)
	{
		UWidgetTree* Tree = Guide->WidgetTree;
		TSet<FKey> SeenKeys;
		TSet<UWidget*> UsedBorders;

		for (const FControlsGuideKeySlot& Known : GetControlsGuideKeySlots())
		{
			FDiscoveredKeycap Slot;
			Slot.Key = Known.PhysicalKey;
			Slot.Border = Cast<UBorder>(Tree->FindWidget(Known.BorderWidgetName));
			Slot.KeyText = Cast<UTextBlock>(Tree->FindWidget(Known.KeyTextWidgetName));
			Slot.Label = Cast<UTextBlock>(Tree->FindWidget(Known.LabelWidgetName));
			if (!Slot.Border && !Slot.KeyText)
			{
				continue; // 조작키 안내 위젯이 아니면 아무것도 안 잡힌다.
			}
			SeenKeys.Add(Slot.Key);
			if (Slot.Border)
			{
				UsedBorders.Add(Slot.Border);
			}
			OutSlots.Add(Slot);
		}

		Tree->ForEachWidget([&SeenKeys, &UsedBorders, &OutSlots](UWidget* W)
		{
			UBorder* Border = Cast<UBorder>(W);
			if (!Border || UsedBorders.Contains(Border))
			{
				return;
			}

			UTextBlock* KeyText = Cast<UTextBlock>(Border->GetContent());
			UTextBlock* Label = nullptr;
			if (!KeyText)
			{
				UVerticalBox* VB = Cast<UVerticalBox>(Border->GetContent());
				if (!VB)
				{
					return;
				}
				for (int32 i = 0; i < VB->GetChildrenCount(); ++i)
				{
					if (UTextBlock* T = Cast<UTextBlock>(VB->GetChildAt(i)))
					{
						if (!KeyText)    { KeyText = T; }
						else if (!Label) { Label = T; }
					}
				}
			}
			if (!KeyText)
			{
				return;
			}

			const FKey Key = KeyFromCapText(KeyText->GetText().ToString());
			if (!Key.IsValid() || IsSystemReservedKey(Key) || SeenKeys.Contains(Key))
			{
				return;
			}
			if (Label && !Label->GetText().IsEmpty() &&
				!Label->GetFName().ToString().StartsWith(DynamicLabelPrefix))
			{
				return;
			}
			SeenKeys.Add(Key);
			OutSlots.Add({ Key, Border, KeyText, Label });
		});
	}

	/** 설정 창 리바인드 검증용: 다이어그램에서 실제 발견된 표시 가능 키 캐시. */
	TSet<FKey> GDisplayableKeyCache;
	bool bDisplayableKeyCacheBuilt = false;

	void UpdateDisplayableKeyCache(const TArray<FDiscoveredKeycap>& Slots)
	{
		GDisplayableKeyCache.Reset();
		for (const FDiscoveredKeycap& Slot : Slots)
		{
			GDisplayableKeyCache.Add(Slot.Key);
		}
		bDisplayableKeyCacheBuilt = true;
	}

	/** 라벨이 없는 키캡에 라벨 TextBlock을 동적으로 만들어 붙인다(필요 시 Border 내용을 VerticalBox로 재구성). */
	UTextBlock* EnsureSlotLabel(UUserWidget* Guide, FDiscoveredKeycap& Slot, const UTextBlock* RefLabel)
	{
		if (Slot.Label || !Slot.Border || !Slot.KeyText || !Guide->WidgetTree)
		{
			return Slot.Label;
		}

		UWidgetTree* Tree = Guide->WidgetTree;
		UVerticalBox* VB = Cast<UVerticalBox>(Slot.Border->GetContent());
		if (!VB)
		{
			// Border > 키텍스트 구조를 Border > VB(키텍스트, 라벨)로 재구성한다.
			VB = Tree->ConstructWidget<UVerticalBox>();
			Slot.KeyText->RemoveFromParent();
			Slot.Border->SetContent(VB);
			if (UVerticalBoxSlot* KeySlot = VB->AddChildToVerticalBox(Slot.KeyText))
			{
				KeySlot->SetHorizontalAlignment(HAlign_Center);
			}
			if (UBorderSlot* BorderSlot = Cast<UBorderSlot>(VB->Slot))
			{
				BorderSlot->SetHorizontalAlignment(HAlign_Fill);
				BorderSlot->SetVerticalAlignment(VAlign_Center);
				BorderSlot->SetPadding(FMargin(1.f));
			}
		}

		const FName LabelName(*(FString(DynamicLabelPrefix) + Slot.Key.ToString()));
		UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), LabelName);
		if (RefLabel)
		{
			Label->SetFont(RefLabel->GetFont());
		}
		Label->SetJustification(ETextJustify::Center);
		Label->SetColorAndOpacity(FSlateColor(FLinearColor(0.961f, 0.922f, 0.851f, 0.8f))); // #F5EBD9CC
		if (UVerticalBoxSlot* LabelSlot = VB->AddChildToVerticalBox(Label))
		{
			LabelSlot->SetHorizontalAlignment(HAlign_Center);
		}
		Slot.Label = Label;
		return Label;
	}
}

bool URetrieveUISettingsLibrary::IsControlsGuideDisplayableKey(const FKey& Key)
{
	if (IsSystemReservedKey(Key))
	{
		return false;
	}
	if (bDisplayableKeyCacheBuilt)
	{
		return GDisplayableKeyCache.Contains(Key);
	}
	// 캐시 전 폴백: 알려진 기본 슬롯 목록.
	for (const FControlsGuideKeySlot& Slot : GetControlsGuideKeySlots())
	{
		if (Slot.PhysicalKey == Key)
		{
			return true;
		}
	}
	return false;
}

void URetrieveUISettingsLibrary::EnsureControlsGuideKeyCache(APlayerController* OwningPlayer)
{
	if (bDisplayableKeyCacheBuilt || !OwningPlayer)
	{
		return;
	}
	UClass* GuideClass = LoadClass<UUserWidget>(
		nullptr, TEXT("/Game/Retrieve/UI/Menu/WBP_ControlsGuide.WBP_ControlsGuide_C"));
	if (!GuideClass)
	{
		return;
	}
	// 뷰포트에 붙이지 않는 임시 인스턴스로 트리 구조만 읽는다(참조를 남기지 않아 GC가 수거).
	UUserWidget* Temp = CreateWidget<UUserWidget>(OwningPlayer, GuideClass);
	if (!Temp || !Temp->WidgetTree)
	{
		return;
	}
	TArray<FDiscoveredKeycap> Slots;
	DiscoverKeycaps(Temp, Slots);
	UpdateDisplayableKeyCache(Slots);
}

void URetrieveUISettingsLibrary::RefreshControlsGuideKeyLabels(UUserWidget* GuideWidget)
{
	if (!GuideWidget || !GuideWidget->WidgetTree)
	{
		return;
	}

	// 다이어그램에 실제 그려진 키캡을 전부 찾는다(알려진 슬롯 + 텍스트 매칭 발견).
	// 설정 창의 리바인드 허용 키 캐시도 이 결과로 갱신된다.
	TArray<FDiscoveredKeycap> Slots;
	DiscoverKeycaps(GuideWidget, Slots);
	UpdateDisplayableKeyCache(Slots);

	ULocalPlayer* LocalPlayer = GuideWidget->GetOwningLocalPlayer();

	// 설정 창을 한 번도 열지 않았어도 안내 위젯이 리바인드 키를 읽을 수 있도록,
	// 프로파일을 조회하기 전에 PlayerMappable/IMC 등록을 여기서 직접 보장한다.
	// (예전엔 이 등록이 설정 패널에만 있어, 설정→조작 탭을 들렀다 와야 안내가 맞게 보였다.)
	URetrieveSettingsPanelWidget::EnsureInputMappingsRegistered(LocalPlayer);

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		LocalPlayer ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
	UEnhancedInputUserSettings* InputSettings = InputSubsystem ? InputSubsystem->GetUserSettings() : nullptr;
	const UEnhancedPlayerMappableKeyProfile* Profile = InputSettings ? InputSettings->GetActiveKeyProfile() : nullptr;
	if (!Profile)
	{
		// 등록을 시도했는데도 프로파일이 없으면(로컬 플레이어/입력 서브시스템 부재 등)
		// WBP에 디자인된 기본 상태가 곧 정답이므로 그대로 둔다.
		return;
	}

	// 다이어그램 범례 색(원본 스펙에서 그대로 추출): 액션 버튼/특수 액션 버튼/특수 기능.
	const FLinearColor ActionColor(0.757f, 0.267f, 0.247f, 1.0f);          // #C1443F
	const FLinearColor SpecialActionColor(0.541f, 0.353f, 0.180f, 1.0f);   // #8A5A2E
	const FLinearColor SpecialFunctionColor(0.431f, 0.310f, 0.627f, 1.0f); // #6E4FA0
	const FLinearColor DefaultBorderColor(0.227f, 0.227f, 0.227f, 0.8f);   // #3A3A3ACC
	const FLinearColor DimKeyTextColor(0.549f, 0.549f, 0.549f, 1.0f);     // #8C8C8CFF
	const FLinearColor BrightKeyTextColor(1.0f, 0.965f, 0.898f, 1.0f);    // #FFF6E5

	// ── 새로 추가한 13개 키(G/H/J/K/Z/X/C/V/B/Y/U/O/P)의 스타일 정규화 ──
	// 이 텍스트 위젯들은 Monolith로 구조만 추가돼 폰트/정렬이 엔진 기본값(큰 흰 글씨, 좌상단)이다.
	// 에디터 편집 없이, 원래 디자인된 키(F=키텍스트 n173, 라벨 n174)의 폰트를 복사해 통일한다.
	{
		const UTextBlock* RefKeyText = Cast<UTextBlock>(GuideWidget->WidgetTree->FindWidget(TEXT("n173")));
		const UTextBlock* RefLabel = Cast<UTextBlock>(GuideWidget->WidgetTree->FindWidget(TEXT("n174")));

		struct FNewKeySlotStyle { FName VBName; FName KeyTxtName; FName LblName; };
		static const FNewKeySlotStyle NewSlots[] = {
			{ TEXT("VB_G"), TEXT("KeyTxt_G"), TEXT("Lbl_G") },
			{ TEXT("VB_H"), TEXT("KeyTxt_H"), TEXT("Lbl_H") },
			{ TEXT("VB_J"), TEXT("KeyTxt_J"), TEXT("Lbl_J") },
			{ TEXT("VB_K"), TEXT("KeyTxt_K"), TEXT("Lbl_K") },
			{ TEXT("VB_Z"), TEXT("KeyTxt_Z"), TEXT("Lbl_Z") },
			{ TEXT("VB_X"), TEXT("KeyTxt_X"), TEXT("Lbl_X") },
			{ TEXT("VB_C"), TEXT("KeyTxt_C"), TEXT("Lbl_C") },
			{ TEXT("VB_V"), TEXT("KeyTxt_V"), TEXT("Lbl_V") },
			{ TEXT("VB_B"), TEXT("KeyTxt_B"), TEXT("Lbl_B") },
			{ TEXT("VB_Y"), TEXT("KeyTxt_Y"), TEXT("Lbl_Y") },
			{ TEXT("VB_U"), TEXT("KeyTxt_U"), TEXT("Lbl_U") },
			{ TEXT("VB_O"), TEXT("KeyTxt_O"), TEXT("Lbl_O") },
			{ TEXT("VB_P"), TEXT("KeyTxt_P"), TEXT("Lbl_P") },
		};
		for (const FNewKeySlotStyle& Slot : NewSlots)
		{
			if (UVerticalBox* VB = Cast<UVerticalBox>(GuideWidget->WidgetTree->FindWidget(Slot.VBName)))
			{
				if (UBorderSlot* BorderSlot = Cast<UBorderSlot>(VB->Slot))
				{
					BorderSlot->SetHorizontalAlignment(HAlign_Fill);
					BorderSlot->SetVerticalAlignment(VAlign_Center);
					BorderSlot->SetPadding(FMargin(1.f));
				}
			}
			if (UTextBlock* KeyTxt = Cast<UTextBlock>(GuideWidget->WidgetTree->FindWidget(Slot.KeyTxtName)))
			{
				if (RefKeyText)
				{
					KeyTxt->SetFont(RefKeyText->GetFont());
				}
				KeyTxt->SetJustification(ETextJustify::Center);
				if (UVerticalBoxSlot* TxtSlot = Cast<UVerticalBoxSlot>(KeyTxt->Slot))
				{
					TxtSlot->SetHorizontalAlignment(HAlign_Center);
				}
			}
			if (UTextBlock* Lbl = Cast<UTextBlock>(GuideWidget->WidgetTree->FindWidget(Slot.LblName)))
			{
				if (RefLabel)
				{
					Lbl->SetFont(RefLabel->GetFont());
				}
				Lbl->SetJustification(ETextJustify::Center);
				Lbl->SetColorAndOpacity(FSlateColor(FLinearColor(0.961f, 0.922f, 0.851f, 0.8f))); // #F5EBD9CC
				if (UVerticalBoxSlot* LblSlot = Cast<UVerticalBoxSlot>(Lbl->Slot))
				{
					LblSlot->SetHorizontalAlignment(HAlign_Center);
				}
			}
		}
	}

	// (마우스로 조작하는 공격, 안내에 아직 없는 강공격은 제외 — 강공격은 default IMC 매핑이 없어
	//  WBP에도 위치가 아예 없다.)
	// 같은 키에 여러 액션이 매핑되면(회피/질주 = Shift 짧게/길게) 라벨을 "/"로 합쳐 표시한다.
	// 배열 순서가 곧 합쳐지는 라벨 순서다(IA_Roll 다음 IA_Sprint → "회피/질주").
	static const FControlsGuideAction Actions[] = {
		{ TEXT("IA_LockOn"),       TEXT("타겟 고정"), ActionColor },
		{ TEXT("IA_Absorb"),       TEXT("흡수"),      ActionColor },
		{ TEXT("IA_Burst"),        TEXT("버스트"),    ActionColor },
		{ TEXT("IA_Guard"),        TEXT("방어"),      ActionColor },
		{ TEXT("IA_Interaction"),  TEXT("상호작용"),  ActionColor },
		{ TEXT("IA_RecallLumen"),  TEXT("루멘"),      SpecialFunctionColor },
		{ TEXT("IA_QuickSlotWheel"), TEXT("퀵슬롯"),  SpecialActionColor },
		{ TEXT("IA_Crouch"),       TEXT("앉기"),      SpecialActionColor },
		{ TEXT("IA_Jump"),         TEXT("점프"),      SpecialActionColor },
		{ TEXT("IA_ElementMode1"), TEXT("원소"),      SpecialActionColor },
		{ TEXT("IA_ElementMode2"), TEXT("원소"),      SpecialActionColor },
		{ TEXT("IA_ElementMode3"), TEXT("원소"),      SpecialActionColor },
		{ TEXT("IA_Roll"),         TEXT("회피"),      SpecialActionColor },
		{ TEXT("IA_Sprint"),       TEXT("질주"),      SpecialActionColor },
	};

	// 1) 발견된 관리 키캡을 전부 기본(무채색) 상태로 리셋한다. 리바인드로 자리를 옮긴 액션이
	//    예전 자리에 라벨을 남기지 않도록, 매번 전체를 지운 뒤 다시 칠한다.
	for (const FDiscoveredKeycap& Slot : Slots)
	{
		if (Slot.Border)
		{
			Slot.Border->SetBrushColor(DefaultBorderColor);
		}
		if (Slot.KeyText)
		{
			Slot.KeyText->SetColorAndOpacity(FSlateColor(DimKeyTextColor));
		}
		if (Slot.Label)
		{
			Slot.Label->SetText(FText::GetEmpty());
		}
	}

	// 2) 각 액션의 현재 바인딩 키를 찾아, 발견된 키캡이면 색+라벨을 입힌다.
	//    라벨 슬롯이 없는 키캡(숫자 6~0, 구두점 등)은 이때 라벨을 동적으로 만들어 붙인다.
	const UTextBlock* RefLabel = Cast<UTextBlock>(GuideWidget->WidgetTree->FindWidget(TEXT("n174")));
	for (const FControlsGuideAction& Act : Actions)
	{
		FKey BoundKey;
		bool bFound = false;
		for (const TPair<FName, FKeyMappingRow>& Pair : Profile->GetPlayerMappingRows())
		{
			for (const FPlayerKeyMapping& Mapping : Pair.Value.Mappings)
			{
				const UInputAction* Action = Mapping.GetAssociatedInputAction();
				if (Action && Action->GetFName() == Act.ActionAssetName && !Mapping.GetCurrentKey().IsGamepadKey())
				{
					BoundKey = Mapping.GetCurrentKey();
					bFound = true;
					break;
				}
			}
			if (bFound)
			{
				break;
			}
		}
		if (!bFound)
		{
			continue;
		}

		for (FDiscoveredKeycap& Slot : Slots)
		{
			if (Slot.Key != BoundKey)
			{
				continue;
			}
			if (Slot.Border)
			{
				Slot.Border->SetBrushColor(Act.CategoryColor);
			}
			if (Slot.KeyText)
			{
				Slot.KeyText->SetColorAndOpacity(FSlateColor(BrightKeyTextColor));
			}
			if (UTextBlock* Label = EnsureSlotLabel(GuideWidget, Slot, RefLabel))
			{
				// 앞선 액션이 이미 이 키를 칠했으면(같은 키 공유: 회피/질주 등) 라벨을 합친다.
				const FString Existing = Label->GetText().ToString();
				const FString NewLabel = (Existing.IsEmpty() || Existing == Act.DisplayLabel)
					? FString(Act.DisplayLabel)
					: Existing + TEXT("/") + Act.DisplayLabel;
				Label->SetText(FText::FromString(NewLabel));
			}
			break;
		}
	}

	// 3) EnhancedInput 액션(IA_*)이 아니라 컨트롤러가 직접 하드코딩 처리하는 고정 시스템 키를
	//    고정 라벨로 표시한다. 스킬 안내(K)는 ARetrievePlayerController::SkillOverviewPanelKey로
	//    처리되어 위 액션 루프에 잡히지 않으므로, 여기서 라벨+색을 직접 입힌다.
	//    (리바인드 불가 → IsSystemReservedKey에도 포함되어 있어 액션이 K를 덮어쓰지 않는다.)
	struct FControlsGuideFixedKey { FKey Key; const TCHAR* DisplayLabel; FLinearColor Color; };
	static const FControlsGuideFixedKey FixedKeys[] = {
		{ EKeys::K, TEXT("스킬 안내"), SpecialFunctionColor },
	};
	for (const FControlsGuideFixedKey& Fixed : FixedKeys)
	{
		for (FDiscoveredKeycap& Slot : Slots)
		{
			if (Slot.Key != Fixed.Key)
			{
				continue;
			}
			if (Slot.Border)
			{
				Slot.Border->SetBrushColor(Fixed.Color);
			}
			if (Slot.KeyText)
			{
				Slot.KeyText->SetColorAndOpacity(FSlateColor(BrightKeyTextColor));
			}
			if (UTextBlock* Label = EnsureSlotLabel(GuideWidget, Slot, RefLabel))
			{
				Label->SetText(FText::FromString(Fixed.DisplayLabel));
			}
			break;
		}
	}

	// 공격/강공격은 마우스 고정(리바인드 불가)이므로 마우스 다이어그램의 텍스트는 WBP 기본값
	// ("공격 (좌클릭)" 등)을 그대로 둔다. 설정 화면에서도 공격 리바인드는 비활성화된다.
}
