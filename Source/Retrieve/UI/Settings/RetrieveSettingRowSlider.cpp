#include "UI/Settings/RetrieveSettingRowSlider.h"
#include "Components/Slider.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanelSlot.h"

void URetrieveSettingRowSlider::NativeConstruct()
{
	Super::NativeConstruct();

	if (Slider)
	{
		Slider->OnValueChanged.AddUniqueDynamic(this, &URetrieveSettingRowSlider::HandleSliderChanged);
	}
	if (ArrowLeft)
	{
		ArrowLeft->OnClicked.AddUniqueDynamic(this, &URetrieveSettingRowSlider::HandleArrowLeft);
	}
	if (ArrowRight)
	{
		ArrowRight->OnClicked.AddUniqueDynamic(this, &URetrieveSettingRowSlider::HandleArrowRight);
	}
	// 값 박스는 입력을 가로채지 않고(트랙 드래그가 슬라이더로 가도록) 위치만 따라간다.
	if (ValBox)
	{
		ValBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	Refresh(Slider ? Slider->GetValue() : 0.f);
}

void URetrieveSettingRowSlider::SetValueSilently(float Value01)
{
	Value01 = FMath::Clamp(Value01, 0.f, 1.f);
	TGuardValue<bool> Guard(bSuppressBroadcast, true);
	if (Slider)
	{
		Slider->SetValue(Value01);
	}
	Refresh(Value01);
}

void URetrieveSettingRowSlider::SetRawValueSilently(float RawValue)
{
	const float Range = RangeMax - RangeMin;
	const float Value01 = FMath::IsNearlyZero(Range) ? 0.f : FMath::Clamp((RawValue - RangeMin) / Range, 0.f, 1.f);
	SetValueSilently(Value01);
}

void URetrieveSettingRowSlider::SetLabelTexts(const FText& Label, const FText& Desc)
{
	if (LabelText)
	{
		LabelText->SetText(Label);
	}
	if (DescText)
	{
		DescText->SetText(Desc);
	}
}

float URetrieveSettingRowSlider::GetValue01() const
{
	return Slider ? Slider->GetValue() : 0.f;
}

float URetrieveSettingRowSlider::GetRawValue() const
{
	return FMath::Lerp(RangeMin, RangeMax, GetValue01());
}

void URetrieveSettingRowSlider::Refresh(float Value01)
{
	Value01 = FMath::Clamp(Value01, 0.f, 1.f);

	if (ValueText)
	{
		if (bDisplayAsPercent)
		{
			ValueText->SetText(FText::AsNumber(FMath::RoundToInt(Value01 * 100.f)));
		}
		else
		{
			FNumberFormattingOptions Opts;
			Opts.MinimumFractionalDigits = DisplayDecimals;
			Opts.MaximumFractionalDigits = DisplayDecimals;
			ValueText->SetText(FText::AsNumber(FMath::Lerp(RangeMin, RangeMax, Value01), &Opts));
		}
	}

	// 값 박스를 트랙상의 값 위치로 이동. 고정 px 대신 상대 앵커(0~1)를 써서
	// 실제 트랙 너비에 자동으로 맞춘다(슬라이더 드래그 위치와 정확히 일치).
	// 정렬 X = Value01 로 두면 값 0=왼쪽 정렬, 1=오른쪽 정렬이라 박스가 트랙 밖으로 나가지 않는다.
	if (ValBox)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(ValBox->Slot))
		{
			// 앵커 X = Value01, 정렬 X = Value01 → 박스가 트랙 경계 밖으로 나가지 않는다.
			// (value=0: 왼쪽 정렬, value=0.5: 중앙 정렬, value=1: 오른쪽 정렬)
			// Slider는 Visible로 유지되어 built-in 드래그가 동작하고,
			// ValBox(HitTestInvisible)가 항상 Slider 영역 안에 있으므로 클릭이 Slider로 전달된다.
			CanvasSlot->SetAnchors(FAnchors(Value01, 0.5f));
			CanvasSlot->SetAlignment(FVector2D(Value01, 0.5f));
			CanvasSlot->SetPosition(FVector2D(0.f, 0.f));
		}
	}
}

void URetrieveSettingRowSlider::HandleSliderChanged(float Value)
{
	Refresh(Value);
	if (!bSuppressBroadcast)
	{
		OnRowValueChanged.Broadcast(RowKey, FMath::Lerp(RangeMin, RangeMax, Value));
	}
}

void URetrieveSettingRowSlider::HandleArrowLeft()
{
	if (Slider)
	{
		const float NewValue = FMath::Clamp(Slider->GetValue() - StepAmount, 0.f, 1.f);
		Slider->SetValue(NewValue);
		HandleSliderChanged(NewValue);
	}
}

void URetrieveSettingRowSlider::HandleArrowRight()
{
	if (Slider)
	{
		const float NewValue = FMath::Clamp(Slider->GetValue() + StepAmount, 0.f, 1.f);
		Slider->SetValue(NewValue);
		HandleSliderChanged(NewValue);
	}
}

