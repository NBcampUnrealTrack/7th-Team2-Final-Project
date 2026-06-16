#include "UI/HUD/RetrieveElementGaugeWidget.h"

#include "Components/Element/ElementGaugeComponent.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "MVVMSubsystem.h"
#include "UObject/ConstructorHelpers.h"
#include "View/MVVMView.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Player/RetrievePlayerController.h"
#include "UI/RetrieveElementUILibrary.h"
#include "UI/ViewModels/ElementGaugeViewModel.h"
#include "UI/ViewModels/HUDViewModel.h"

// ─────────────────────────── NativeConstruct ─────────────────────────────────

URetrieveElementGaugeWidget::URetrieveElementGaugeWidget()
{
	static ConstructorHelpers::FObjectFinder<UDataTable> BuffTableFinder(
		TEXT("/Game/Retrieve/Data/Skill/DT_BuffDefinitions.DT_BuffDefinitions"));
	if (BuffTableFinder.Succeeded())
	{
		BuffDefinitionTable = BuffTableFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UDataTable> SkillCombinationTableFinder(
		TEXT("/Game/Retrieve/Data/Skill/DT_SkillCombination.DT_SkillCombination"));
	if (SkillCombinationTableFinder.Succeeded())
	{
		SkillCombinationTable = SkillCombinationTableFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SkillIconMaterialFinder(
		TEXT("/Game/Retrieve/UI/Materials/M_UI_SkillIcon_Masked.M_UI_SkillIcon_Masked"));
	if (SkillIconMaterialFinder.Succeeded())
	{
		SkillIconMaskedMaterial = SkillIconMaterialFinder.Object;
	}
}

void URetrieveElementGaugeWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ARetrievePlayerController* RPC = Cast<ARetrievePlayerController>(GetOwningPlayer());
	if (!RPC) return;

	UHUDViewModel* HUDVM = RPC->GetHUDViewModel();
	if (!HUDVM) return;

	UElementGaugeViewModel* GaugeVM = HUDVM->GetElementGauge();
	if (!GaugeVM) return;

	// deprecated source 바인딩이 auto-create한 VM을 올바른 인스턴스로 교체
	if (UMVVMSubsystem* MVVM = GEngine ? GEngine->GetEngineSubsystem<UMVVMSubsystem>() : nullptr)
	{
		if (UMVVMView* View = MVVM->GetViewFromUserWidget(this))
		{
			View->SetViewModel(TEXT("ElementGauge"), GaugeVM);
			View->SetViewModel(TEXT("ElementGaugeViewModel"), GaugeVM);
		}
	}

	InitFromViewModel(GaugeVM);

	// 매 프레임 Percent 보간 + GlowPower 펄스 처리
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::TickAnimation));
}

void URetrieveElementGaugeWidget::NativeDestruct()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}

	if (UElementGaugeViewModel* VM = BoundViewModel.Get())
	{
		VM->OnSlotsUpdated.RemoveDynamic(this, &URetrieveElementGaugeWidget::HandleGaugeUpdated);
		// OnCurrentElementChanged 해제는 URetrieveElementAwareWidget::NativeDestruct에서 처리
	}
	Super::NativeDestruct();
}

// ─────────────────────────── 초기화 ──────────────────────────────────────────

void URetrieveElementGaugeWidget::InitFromViewModel(UElementGaugeViewModel* GaugeVM)
{
	BoundViewModel = GaugeVM;

	GaugeVM->OnSlotsUpdated.RemoveDynamic(this, &URetrieveElementGaugeWidget::HandleGaugeUpdated);
	GaugeVM->OnSlotsUpdated.AddDynamic(this, &URetrieveElementGaugeWidget::HandleGaugeUpdated);
	// OnCurrentElementChanged 구독은 URetrieveElementAwareWidget::NativeConstruct에서 일원화

	// 초기값 즉시 반영 (애니메이션 없이 스냅)
	UpdateSlot(0, GaugeVM->GetSlot0Ratio(), GaugeVM->GetSlot0Element(), GaugeVM->GetSlot0IsFull(), /*bImmediate=*/true);
	UpdateSlot(1, GaugeVM->GetSlot1Ratio(), GaugeVM->GetSlot1Element(), GaugeVM->GetSlot1IsFull(), /*bImmediate=*/true);
	UpdateSlot(2, GaugeVM->GetSlot2Ratio(), GaugeVM->GetSlot2Element(), GaugeVM->GetSlot2IsFull(), /*bImmediate=*/true);
	TriggerElementModePulse(GaugeVM->GetCurrentElement(), /*bImmediate=*/true);
	UpdateSkillIcons();
}

// ─────────────────────────── 슬롯 갱신 ───────────────────────────────────────

void URetrieveElementGaugeWidget::HandleGaugeUpdated()
{
	UElementGaugeViewModel* VM = BoundViewModel.Get();
	if (!VM) return;

	UpdateSlot(0, VM->GetSlot0Ratio(), VM->GetSlot0Element(), VM->GetSlot0IsFull());
	UpdateSlot(1, VM->GetSlot1Ratio(), VM->GetSlot1Element(), VM->GetSlot1IsFull());
	UpdateSlot(2, VM->GetSlot2Ratio(), VM->GetSlot2Element(), VM->GetSlot2IsFull());
	UpdateSkillIcons();
}

void URetrieveElementGaugeWidget::NativeOnElementModeChanged(FGameplayTag NewElement)
{
	TriggerElementModePulse(NewElement, /*bImmediate=*/false);
	UpdateSkillIcons();
	Super::NativeOnElementModeChanged(NewElement);
}

void URetrieveElementGaugeWidget::UpdateSlot(int32 SlotIndex, float Ratio, FGameplayTag Element, bool bFull, bool bImmediate)
{
	// ── Image + DMI 경로 ────────────────────────────────────────────────────
	if (UImage* Img = GetSlotImage(SlotIndex))
	{
		// 원소가 바뀌었을 때만 DMI 교체
		if (!SlotDMIs[SlotIndex] || CachedElements[SlotIndex] != Element)
		{
			CachedElements[SlotIndex] = Element;
			if (UMaterialInterface* MI = GetMIForElement(Element))
			{
				SlotDMIs[SlotIndex] = UMaterialInstanceDynamic::Create(MI, this);
				Img->SetBrushFromMaterial(SlotDMIs[SlotIndex]);
			}
		}

		if (bImmediate)
		{
			// 초기화: 보간 없이 즉시 반영, 글로우 0으로 초기화
			CurrentRatios[SlotIndex] = Ratio;
			TargetRatios[SlotIndex]  = Ratio;
			bPrevFull[SlotIndex]     = bFull;
			bGlowActive[SlotIndex]   = false;

			if (SlotDMIs[SlotIndex])
			{
				SlotDMIs[SlotIndex]->SetScalarParameterValue(TEXT("Percent"),   Ratio);
				SlotDMIs[SlotIndex]->SetScalarParameterValue(TEXT("GlowPower"), 0.f);
			}
		}
		else
		{
			// 충전량 증가 → 충전 펄스
			if (Ratio > TargetRatios[SlotIndex] + 0.01f)
			{
				TriggerGlowPulse(SlotIndex, /*bIsFullTransition=*/false);
			}

			// 슬롯 확정(false → true) → 확정 펄스 (더 강하고 길다)
			if (bFull && !bPrevFull[SlotIndex])
			{
				TriggerGlowPulse(SlotIndex, /*bIsFullTransition=*/true);
			}

			TargetRatios[SlotIndex] = Ratio;
			bPrevFull[SlotIndex]    = bFull;
		}
		return;
	}

	// ── ProgressBar 폴백 (Image_Fill_* 없을 때 사용) ─────────────────────
	if (UProgressBar* PB = GetSlotProgressBar(SlotIndex))
	{
		PB->SetPercent(Ratio);
		PB->SetFillColorAndOpacity(URetrieveElementUILibrary::ElementTagToColor(Element));
	}
}

// ─────────────────────────── 애니메이션 Ticker ────────────────────────────────

bool URetrieveElementGaugeWidget::TickAnimation(float DeltaTime)
{
	// Percent 보간 속도: 초당 6배 거리 좁힘 (0→1 약 0.4 s)
	constexpr float InterpSpeed = 6.f;

	for (int32 i = 0; i < SlotCount; ++i)
	{
		UMaterialInstanceDynamic* DMI = SlotDMIs[i];
		if (!DMI) continue;

		// ── Percent 부드러운 보간 ────────────────────────────────────────
		if (!FMath::IsNearlyEqual(CurrentRatios[i], TargetRatios[i], 0.001f))
		{
			CurrentRatios[i] = FMath::FInterpTo(CurrentRatios[i], TargetRatios[i], DeltaTime, InterpSpeed);
			DMI->SetScalarParameterValue(TEXT("Percent"), CurrentRatios[i]);
		}

		// ── GlowPower 펄스 (sin 곡선: 0 → Peak → 0) ─────────────────────
		// GlowPower 파라미터가 머티리얼에 없으면 SetScalarParameterValue가
		// 무시(silent fail)되므로 안전하다.
		if (bGlowActive[i])
		{
			GlowProgress[i] += DeltaTime;

			if (GlowProgress[i] >= GlowDuration[i])
			{
				bGlowActive[i]  = false;
				GlowProgress[i] = 0.f;
				DMI->SetScalarParameterValue(TEXT("GlowPower"), 0.f);
			}
			else
			{
				// sin(0) = 0, sin(PI/2) = 1, sin(PI) = 0 → 자연스러운 펄스
				const float NormalizedT = GlowProgress[i] / GlowDuration[i];
				const float GlowValue   = GlowPeaks[i] * FMath::Sin(NormalizedT * PI);
				DMI->SetScalarParameterValue(TEXT("GlowPower"), GlowValue);
			}
		}
	}

	if (bElementModePulseActive)
	{
		ElementModePulseProgress += DeltaTime;

		if (ElementModePulseProgress >= ElementModePulseDuration)
		{
			bElementModePulseActive = false;
			ElementModePulseProgress = 0.f;
			SetElementIconVisualState(0.f);
		}
		else
		{
			const float NormalizedT = ElementModePulseProgress / ElementModePulseDuration;
			SetElementIconVisualState(FMath::Sin(NormalizedT * PI));
		}
	}

	return true;  // false를 반환하면 Ticker가 제거되므로 반드시 true
}

void URetrieveElementGaugeWidget::TriggerGlowPulse(int32 SlotIndex, bool bIsFullTransition)
{
	bGlowActive[SlotIndex]  = true;
	GlowProgress[SlotIndex] = 0.f;

	if (bIsFullTransition)
	{
		// 슬롯 확정: 더 밝고(피크 3.5) 더 긴(0.6 s) 펄스
		GlowDuration[SlotIndex] = 0.6f;
		GlowPeaks[SlotIndex]    = 3.5f;
	}
	else
	{
		// 충전 증가: 일반 펄스 (피크 1.8, 0.4 s)
		GlowDuration[SlotIndex] = 0.4f;
		GlowPeaks[SlotIndex]    = 1.8f;
	}
}

// ─────────────────────────── 유틸 ────────────────────────────────────────────

void URetrieveElementGaugeWidget::TriggerElementModePulse(FGameplayTag NewElement, bool bImmediate)
{
	if (!bImmediate && CachedCurrentElement == NewElement)
	{
		return;
	}

	CachedCurrentElement = NewElement;
	ElementModePulseColor = URetrieveElementUILibrary::ElementTagToColor(NewElement);

	if (bImmediate)
	{
		bElementModePulseActive = false;
		ElementModePulseProgress = 0.f;
		SetElementIconVisualState(0.f);
		return;
	}

	bElementModePulseActive = true;
	ElementModePulseProgress = 0.f;
	SetElementIconVisualState(1.f);
}

void URetrieveElementGaugeWidget::SetElementIconVisualState(float PulseAlpha)
{
	if (!Image_Element)
	{
		return;
	}

	const float Scale = 1.f + (0.18f * PulseAlpha);
	Image_Element->SetRenderScale(FVector2D(Scale, Scale));
	Image_Element->SetColorAndOpacity(FLinearColor::LerpUsingHSV(FLinearColor::White, ElementModePulseColor, 0.35f + (0.45f * PulseAlpha)));
	Image_Element->SetRenderOpacity(0.85f + (0.15f * PulseAlpha));
}

void URetrieveElementGaugeWidget::UpdateSkillIcons()
{
	EnsureSkillIconTables();

	FRetrieveBuffUIRow AbsorbRow;
	const UElementGaugeViewModel* VM = BoundViewModel.Get();
	const FGameplayTag AbsorbElement = (VM && VM->GetSlot0IsFull())
		? VM->GetSlot0Element()
		: RetrieveGameplayTags::Element_None;
	const FGameplayTag AbsorbUITag = ResolveAbsorbBuffUITag(AbsorbElement);
	const bool bHasAbsorbRow = URetrieveElementUILibrary::GetBuffUIRow(BuffDefinitionTable, AbsorbUITag, AbsorbRow);
	if (!bHasAbsorbRow && AbsorbUITag.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[ElementGauge] Absorb icon row not found. Table=%s Tag=%s"),
			*GetNameSafe(BuffDefinitionTable), *AbsorbUITag.ToString());
	}
	ApplySkillIcon(Image_AbsorbSkillIcon, AbsorbSkillIconMID, bHasAbsorbRow ? &AbsorbRow : nullptr, bHasAbsorbRow);

	FRetrieveBuffUIRow BurstRow;
	const bool bHasBurstRow = ResolveBurstBuffUIRow(BurstRow);
	ApplySkillIcon(Image_BurstSkillIcon, BurstSkillIconMID, bHasBurstRow ? &BurstRow : nullptr, bHasBurstRow);
}

void URetrieveElementGaugeWidget::EnsureSkillIconTables()
{
	if (!BuffDefinitionTable)
	{
		BuffDefinitionTable = LoadObject<UDataTable>(
			nullptr,
			TEXT("/Game/Retrieve/Data/Skill/DT_BuffDefinitions.DT_BuffDefinitions"));
	}

	if (!SkillCombinationTable)
	{
		SkillCombinationTable = LoadObject<UDataTable>(
			nullptr,
			TEXT("/Game/Retrieve/Data/Skill/DT_SkillCombination.DT_SkillCombination"));
	}
}

FGameplayTag URetrieveElementGaugeWidget::ResolveAbsorbBuffUITag(FGameplayTag ElementTag) const
{
	if (ElementTag.MatchesTagExact(RetrieveGameplayTags::Element_Fire))
	{
		return RetrieveGameplayTags::UI_Buff_Absorb_Fire;
	}
	if (ElementTag.MatchesTagExact(RetrieveGameplayTags::Element_Water))
	{
		return RetrieveGameplayTags::UI_Buff_Absorb_Water;
	}
	if (ElementTag.MatchesTagExact(RetrieveGameplayTags::Element_Wind))
	{
		return RetrieveGameplayTags::UI_Buff_Absorb_Wind;
	}
	return FGameplayTag();
}

bool URetrieveElementGaugeWidget::BuildCurrentBurstPattern(TMap<FGameplayTag, int32>& OutPattern) const
{
	OutPattern.Reset();

	if (const APawn* OwningPawn = GetOwningPlayerPawn())
	{
		if (const UElementGaugeComponent* Gauge = OwningPawn->FindComponentByClass<UElementGaugeComponent>())
		{
			if (!Gauge->IsFull())
			{
				return false;
			}

			OutPattern = Gauge->GetCurrentCombination();
			return !OutPattern.IsEmpty();
		}
	}

	UElementGaugeViewModel* VM = BoundViewModel.Get();
	if (!VM || !VM->GetIsGaugeFull())
	{
		return false;
	}

	auto AddSlot = [&OutPattern](bool bFull, FGameplayTag Element)
	{
		if (bFull && Element.IsValid() && !Element.MatchesTagExact(RetrieveGameplayTags::Element_None))
		{
			OutPattern.FindOrAdd(Element)++;
		}
	};

	AddSlot(VM->GetSlot0IsFull(), VM->GetSlot0Element());
	AddSlot(VM->GetSlot1IsFull(), VM->GetSlot1Element());
	AddSlot(VM->GetSlot2IsFull(), VM->GetSlot2Element());

	return OutPattern.Num() > 0;
}

bool URetrieveElementGaugeWidget::ResolveBurstBuffUIRow(FRetrieveBuffUIRow& OutRow) const
{
	TMap<FGameplayTag, int32> ElementPattern;
	if (!BuildCurrentBurstPattern(ElementPattern))
	{
		return false;
	}

	FSkillCombination Combination;
	if (!URetrieveElementUILibrary::GetMatchingBurstCombination(SkillCombinationTable, ElementPattern, Combination))
	{
		UE_LOG(LogTemp, Warning, TEXT("[ElementGauge] Burst combination row not found. Table=%s PatternCount=%d"),
			*GetNameSafe(SkillCombinationTable), ElementPattern.Num());
		return false;
	}

	const bool bFoundRow = URetrieveElementUILibrary::GetBuffUIRow(BuffDefinitionTable, Combination.BurstUITag, OutRow);
	if (!bFoundRow)
	{
		UE_LOG(LogTemp, Warning, TEXT("[ElementGauge] Burst icon row not found. Table=%s Tag=%s"),
			*GetNameSafe(BuffDefinitionTable), *Combination.BurstUITag.ToString());
	}
	return bFoundRow;
}

void URetrieveElementGaugeWidget::ApplySkillIcon(UImage* Image, TObjectPtr<UMaterialInstanceDynamic>& IconMID, const FRetrieveBuffUIRow* Row, bool bEnabled)
{
	if (!Image)
	{
		return;
	}

	if (!Row || Row->Icon.IsNull())
	{
		Image->SetVisibility(ESlateVisibility::Collapsed);
		Image->SetBrushFromTexture(nullptr, false);
		IconMID = nullptr;
		return;
	}

	UTexture2D* IconTexture = Row->Icon.LoadSynchronous();
	if (!IconTexture)
	{
		Image->SetVisibility(ESlateVisibility::Collapsed);
		Image->SetBrushFromTexture(nullptr, false);
		IconMID = nullptr;
		return;
	}

	if (!IconMID && SkillIconMaskedMaterial)
	{
		IconMID = UMaterialInstanceDynamic::Create(SkillIconMaskedMaterial, this);
	}

	if (IconMID)
	{
		Image->SetBrushFromMaterial(IconMID);
		Image->SetDesiredSizeOverride(FVector2D(96.0f, 96.0f));

		IconMID->SetTextureParameterValue(TEXT("IconTexture"), IconTexture);
		IconMID->SetTextureParameterValue(TEXT("Texture"), IconTexture);
		IconMID->SetTextureParameterValue(TEXT("SkillIcon"), IconTexture);
		IconMID->SetTextureParameterValue(TEXT("Icon"), IconTexture);
	}
	else
	{
		Image->SetBrushFromTexture(IconTexture, false);
	}

	Image->SetColorAndOpacity(Row->TintColor);
	Image->SetRenderOpacity(bEnabled ? 1.0f : 0.35f);
	Image->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

UImage* URetrieveElementGaugeWidget::GetSlotImage(int32 SlotIndex) const
{
	switch (SlotIndex)
	{
	case 0: return Image_Fill_0;
	case 1: return Image_Fill_1;
	case 2: return Image_Fill_2;
	default: return nullptr;
	}
}

UProgressBar* URetrieveElementGaugeWidget::GetSlotProgressBar(int32 SlotIndex) const
{
	switch (SlotIndex)
	{
	case 0: return ProgressBar_Slot0;
	case 1: return ProgressBar_Slot1;
	case 2: return ProgressBar_Slot2;
	default: return nullptr;
	}
}

UMaterialInterface* URetrieveElementGaugeWidget::GetMIForElement(const FGameplayTag& Element) const
{
	if (Element == RetrieveGameplayTags::Element_Fire)  return MI_Fire;
	if (Element == RetrieveGameplayTags::Element_Water) return MI_Water;
	if (Element == RetrieveGameplayTags::Element_Wind)  return MI_Wind;
	return MI_Empty;
}
