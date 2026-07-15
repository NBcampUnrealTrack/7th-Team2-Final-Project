#include "UI/HUD/RetrieveBuffSlotWidget.h"

#include "GameplayEffect.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "UObject/FieldIterator.h"
#include "UObject/UnrealType.h"

void URetrieveBuffSlotWidget::SetBuff(const FRetrieveUIBuffPayload& Payload)
{
	bActive = true;
	ActiveBuffId = Payload.BuffId;

	const FLinearColor StatusColor = ResolveStatusColor(Payload.bIsDebuff, Payload.TintColor);

	if (IMG_Icon)
	{
		if (Payload.Icon)
		{
			IMG_Icon->SetBrushFromTexture(Payload.Icon);
		}
		// Preserve the authored icon colors. Status tint is reserved for the duration bar.
		IMG_Icon->SetColorAndOpacity(FLinearColor::White);
	}

	InitialDuration = Payload.Duration;
	SetBarFillColor(DurationBar, StatusColor);
	SetBarFillAmount(DurationBar, 1.f);

	UpdateDuration(Payload.Duration);
	UpdateStack(1);   // 새 슬롯 배정 시 스택 카운트 초기화 (Bar에서 이후 UpdateStack으로 덮어씀)
	SetToolTipText(BuildTooltipText(Payload));
	SetRenderOpacity(1.f);
	// 커서가 있을 때(메뉴/UI 모드) 호버 툴팁이 뜨도록 히트테스트를 허용한다.
	// SelfHitTestInvisible이면 툴팁 텍스트를 설정해도 절대 표시되지 않는다.
	SetVisibility(ESlateVisibility::Visible);
}

void URetrieveBuffSlotWidget::UpdateDuration(float Remaining)
{
	if (TXT_Duration)
	{
		if (Remaining > 0.f)
		{
			TXT_Duration->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), Remaining)));
		}
		else
		{
			TXT_Duration->SetText(FText::GetEmpty());
		}
	}

	if (InitialDuration > 0.f)
	{
		SetBarFillAmount(DurationBar, FMath::Clamp(Remaining / InitialDuration, 0.f, 1.f));
	}
}

void URetrieveBuffSlotWidget::UpdateStack(int32 Count)
{
	if (!TXT_StackCount)
	{
		return;
	}

	if (Count >= 2)
	{
		TXT_StackCount->SetText(FText::FromString(FString::Printf(TEXT("×%d"), Count)));
		TXT_StackCount->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	else
	{
		TXT_StackCount->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void URetrieveBuffSlotWidget::ClearBuff()
{
	bActive = false;
	ActiveBuffId = FGameplayTag::EmptyTag;
	InitialDuration = 0.f;
	SetBarFillAmount(DurationBar, 1.f);
	SetToolTipText(FText::GetEmpty());
	UpdateStack(0);
	SetVisibility(ESlateVisibility::Collapsed);
}

FLinearColor URetrieveBuffSlotWidget::ResolveStatusColor(bool bIsDebuff, const FLinearColor& DataColor)
{
	if (!DataColor.Equals(FLinearColor::White))
	{
		return DataColor;
	}

	return bIsDebuff
		? FLinearColor(1.f, 0.12f, 0.08f, 1.f)
		: FLinearColor(0.05f, 0.45f, 1.f, 1.f);
}

FText URetrieveBuffSlotWidget::BuildTooltipText(const FRetrieveUIBuffPayload& Payload)
{
	TArray<FString> Lines;

	if (!Payload.DisplayName.IsEmpty())
	{
		Lines.Add(Payload.DisplayName.ToString());
	}
	else
	{
		Lines.Add(Payload.BuffId.ToString());
	}

	if (!Payload.Description.IsEmpty())
	{
		Lines.Add(Payload.Description.ToString());
	}

	if (!Payload.EffectSummary.IsEmpty())
	{
		Lines.Add(Payload.EffectSummary.ToString());
	}

	if (Payload.LinkedGameplayEffect)
	{
		Lines.Add(FString::Printf(TEXT("GE: %s"), *GetNameSafe(Payload.LinkedGameplayEffect.Get())));
	}

	return FText::FromString(FString::Join(Lines, TEXT("\n")));
}

void URetrieveBuffSlotWidget::SetBarFillAmount(UUserWidget* Widget, float Amount)
{
	if (!IsValid(Widget)) return;
	const float ClampedAmount = FMath::Clamp(Amount, 0.f, 1.f);

	if (FDoubleProperty* Prop = CastField<FDoubleProperty>(
			Widget->GetClass()->FindPropertyByName(TEXT("FillAmount"))))
	{
		Prop->SetPropertyValue_InContainer(Widget, static_cast<double>(ClampedAmount));
	}
	else if (FFloatProperty* FloatProp = CastField<FFloatProperty>(
			Widget->GetClass()->FindPropertyByName(TEXT("FillAmount"))))
	{
		FloatProp->SetPropertyValue_InContainer(Widget, ClampedAmount);
	}

	if (UFunction* SetFillAmountFunction = Widget->FindFunction(TEXT("SetFillAmount")))
	{
		TArray<uint8> Params;
		Params.SetNumZeroed(SetFillAmountFunction->ParmsSize);

		for (TFieldIterator<FProperty> It(SetFillAmountFunction); It; ++It)
		{
			FProperty* ParamProp = *It;
			if (!ParamProp->HasAnyPropertyFlags(CPF_Parm) || ParamProp->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				continue;
			}

			if (FDoubleProperty* DoubleParam = CastField<FDoubleProperty>(ParamProp))
			{
				DoubleParam->SetPropertyValue_InContainer(Params.GetData(), static_cast<double>(ClampedAmount));
				break;
			}
			if (FFloatProperty* FloatParam = CastField<FFloatProperty>(ParamProp))
			{
				FloatParam->SetPropertyValue_InContainer(Params.GetData(), ClampedAmount);
				break;
			}
		}

		Widget->ProcessEvent(SetFillAmountFunction, Params.GetData());
	}

	CallBarUpdate(Widget);
}

void URetrieveBuffSlotWidget::SetBarFillColor(UUserWidget* Widget, const FLinearColor& Color)
{
	if (!IsValid(Widget)) return;
	if (FStructProperty* Prop = CastField<FStructProperty>(
			Widget->GetClass()->FindPropertyByName(TEXT("Color_Fill"))))
	{
		FLinearColor* ColorPtr = Prop->ContainerPtrToValuePtr<FLinearColor>(Widget);
		if (ColorPtr) *ColorPtr = Color;
	}

	CallBarUpdate(Widget);
}

void URetrieveBuffSlotWidget::CallBarUpdate(UUserWidget* Widget)
{
	if (!IsValid(Widget)) return;

	if (UFunction* UpdateFunction = Widget->FindFunction(TEXT("UpdateWidget")))
	{
		Widget->ProcessEvent(UpdateFunction, nullptr);
	}
}
