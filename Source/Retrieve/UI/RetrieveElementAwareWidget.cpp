#include "UI/RetrieveElementAwareWidget.h"
#include "Player/RetrievePlayerController.h"
#include "UI/ViewModels/HUDViewModel.h"
#include "UI/ViewModels/ElementGaugeViewModel.h"
#include "UI/RetrieveUISettingsLibrary.h"
#include "Settings/RetrieveSettingsSubsystem.h"

void URetrieveElementAwareWidget::NativeConstruct()
{
	// BP Event Construct 이전에 구독 → GetCurrentActiveElement()가 Construct 시점에 유효
	ARetrievePlayerController* RPC = Cast<ARetrievePlayerController>(GetOwningPlayer());
	if (RPC)
	{
		UHUDViewModel* HUDVM = RPC->GetHUDViewModel();
		if (HUDVM)
		{
			UElementGaugeViewModel* GaugeVM = HUDVM->GetElementGauge();
			if (GaugeVM)
			{
				BoundElementVM = GaugeVM;
				GaugeVM->OnCurrentElementChanged.AddDynamic(this, &ThisClass::HandleElementModeChanged);
			}
		}
	}

	Super::NativeConstruct(); // BP Event Construct 발화 — 이때 GetCurrentActiveElement() 사용 가능

	// BP Construct 완료 후 초기 원소 dispatch
	if (UElementGaugeViewModel* VM = BoundElementVM.Get())
	{
		NativeOnElementModeChanged(VM->GetCurrentElement());
	}

	// Reduce Motion: 접근성 설정 구독 + 현재 상태 즉시 적용.
	if (URetrieveSettingsSubsystem* Subsystem = URetrieveSettingsSubsystem::Get(this))
	{
		Subsystem->OnSettingChanged.AddUniqueDynamic(this, &ThisClass::HandleAccessibilitySettingChanged);
	}
	ApplyReduceMotion();
}

FGameplayTag URetrieveElementAwareWidget::GetCurrentActiveElement() const
{
	if (const UElementGaugeViewModel* VM = BoundElementVM.Get())
	{
		return VM->GetCurrentElement();
	}
	return FGameplayTag::EmptyTag;
}

void URetrieveElementAwareWidget::NativeDestruct()
{
	if (UElementGaugeViewModel* VM = BoundElementVM.Get())
	{
		VM->OnCurrentElementChanged.RemoveDynamic(this, &ThisClass::HandleElementModeChanged);
	}
	if (URetrieveSettingsSubsystem* Subsystem = URetrieveSettingsSubsystem::Get(this))
	{
		Subsystem->OnSettingChanged.RemoveDynamic(this, &ThisClass::HandleAccessibilitySettingChanged);
	}
	Super::NativeDestruct();
}

void URetrieveElementAwareWidget::HandleAccessibilitySettingChanged(ERetrieveSettingsCategory Category)
{
	if (Category == ERetrieveSettingsCategory::Accessibility || Category == ERetrieveSettingsCategory::MAX)
	{
		ApplyReduceMotion();
	}
}

void URetrieveElementAwareWidget::ApplyReduceMotion()
{
	// Reduce Motion이 켜져 있으면 장식 애니메이션을 정지한다(데이터 갱신은 SetPercent/SetText라 무관).
	// 해제는 위젯 재생성 시 복원.
	if (URetrieveUISettingsLibrary::IsReduceMotionEnabled())
	{
		URetrieveUISettingsLibrary::StopAnimationsRecursive(this);
	}
}

void URetrieveElementAwareWidget::HandleElementModeChanged(FGameplayTag NewElement)
{
	NativeOnElementModeChanged(NewElement);
}

void URetrieveElementAwareWidget::NativeOnElementModeChanged(FGameplayTag NewElement)
{
	DispatchElementModeChangedToBlueprint(NewElement);
}

void URetrieveElementAwareWidget::DispatchElementModeChangedToBlueprint(FGameplayTag NewElement)
{
	UFunction* EventFunction = FindFunction(TEXT("BP_OnElementModeChanged"));
	if (!EventFunction)
	{
		return;
	}

	struct FParams { FGameplayTag NewElementTag; };
	FParams Params;
	Params.NewElementTag = NewElement;
	ProcessEvent(EventFunction, &Params);
}
