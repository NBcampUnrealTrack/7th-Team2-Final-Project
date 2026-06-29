#include "UI/VFX/RetrieveUIVFXWidget.h"

#include "Components/Button.h"
#include "Components/Widget.h"
#include "Curves/CurveFloat.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "UI/Sound/RetrieveUISoundButtonBinder.h"
#include "UI/Sound/RetrieveUISoundSubsystem.h"
#include "UI/Sound/RetrieveUISoundTypes.h"
#include "UI/VFX/RetrieveUIVFXProfile.h"
#include "Settings/RetrieveGameUserSettings.h"

URetrieveUIVFXWidget::URetrieveUIVFXWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void URetrieveUIVFXWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!VFXProfile)
	{
		VFXProfile = LoadObject<URetrieveUIVFXProfile>(
			nullptr,
			TEXT("/Game/Retrieve/UI/VFX/DA_UIVFX_Default.DA_UIVFX_Default"));
	}

	if (!DefaultVFXTarget)
	{
		DefaultVFXTarget = GetRootWidget();
	}
}

void URetrieveUIVFXWidget::NativeDestruct()
{
	for (TObjectPtr<UObject>& BinderObject : SoundButtonBinders)
	{
		if (URetrieveUISoundButtonBinder* Binder = Cast<URetrieveUISoundButtonBinder>(BinderObject))
		{
			Binder->Unbind();
		}
	}
	SoundButtonBinders.Empty();

	Super::NativeDestruct();
}

void URetrieveUIVFXWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	for (int32 Index = ActiveVFX.Num() - 1; Index >= 0; --Index)
	{
		FRetrieveActiveUIVFX& Active = ActiveVFX[Index];
		UWidget* TargetWidget = Active.TargetWidget.Get();
		if (!TargetWidget)
		{
			ActiveVFX.RemoveAtSwap(Index);
			continue;
		}

		Active.ElapsedTime += InDeltaTime;
		const float Duration = FMath::Max(Active.Preset.Duration, KINDA_SMALL_NUMBER);
		const float NormalizedTime = FMath::Clamp(Active.ElapsedTime / Duration, 0.0f, 1.0f);
		const float Alpha = Active.Preset.Curve
			? Active.Preset.Curve->GetFloatValue(NormalizedTime)
			: NormalizedTime;

		ApplyPresetAtAlpha(TargetWidget, Active.Preset, Alpha);

		if (Active.ElapsedTime >= Duration)
		{
			const FGameplayTag FinishedTag = Active.RequestedEffectTag;
			const bool bNotify = Active.bNotifyWhenFinished;
			ActiveVFX.RemoveAtSwap(Index);

			if (bNotify)
			{
				OnUIVFXFinished.Broadcast(FinishedTag);
			}
		}
	}
}

bool URetrieveUIVFXWidget::PlayUIVFX(FGameplayTag EffectTag, bool bNotifyWhenFinished)
{
	return PlayUIVFXOnWidget(EffectTag, ResolveDefaultVFXTarget(), bNotifyWhenFinished);
}

bool URetrieveUIVFXWidget::PlayUIVFXOnWidget(FGameplayTag EffectTag, UWidget* TargetWidget, bool bNotifyWhenFinished)
{
	if (const URetrieveGameUserSettings* Settings = URetrieveGameUserSettings::Get();
		Settings && Settings->bReduceMotion)
	{
		if (bNotifyWhenFinished)
		{
			OnUIVFXFinished.Broadcast(EffectTag);
		}
		return false;
	}

	if (!VFXProfile || !TargetWidget || !EffectTag.IsValid())
	{
		if (bNotifyWhenFinished)
		{
			OnUIVFXFinished.Broadcast(EffectTag);
		}
		return false;
	}

	FRetrieveUIVFXPreset Preset;
	if (!VFXProfile->GetPreset(EffectTag, Preset))
	{
		if (bNotifyWhenFinished)
		{
			OnUIVFXFinished.Broadcast(EffectTag);
		}
		return false;
	}

	StopUIVFX(TargetWidget);

	FRetrieveActiveUIVFX& Active = ActiveVFX.AddDefaulted_GetRef();
	Active.TargetWidget = TargetWidget;
	Active.Preset = Preset;
	Active.RequestedEffectTag = EffectTag;
	Active.ElapsedTime = 0.0f;
	Active.bNotifyWhenFinished = bNotifyWhenFinished;

	ApplyPresetAtAlpha(TargetWidget, Preset, 0.0f);

	if (Preset.Duration <= KINDA_SMALL_NUMBER)
	{
		ApplyPresetAtAlpha(TargetWidget, Preset, 1.0f);
		ActiveVFX.Pop(EAllowShrinking::No);
		if (bNotifyWhenFinished)
		{
			OnUIVFXFinished.Broadcast(EffectTag);
		}
	}

	return true;
}

void URetrieveUIVFXWidget::StopUIVFX(UWidget* TargetWidget)
{
	if (!TargetWidget)
	{
		return;
	}

	for (int32 Index = ActiveVFX.Num() - 1; Index >= 0; --Index)
	{
		if (ActiveVFX[Index].TargetWidget.Get() == TargetWidget)
		{
			ActiveVFX.RemoveAtSwap(Index);
		}
	}
}

bool URetrieveUIVFXWidget::PlayButtonHoverVFX(UWidget* TargetWidget)
{
	PlayUISound(ERetrieveUISoundEvent::Hover);
	return PlayUIVFXOnWidget(RetrieveGameplayTags::UI_VFX_Button_Hover, TargetWidget);
}

bool URetrieveUIVFXWidget::PlayButtonUnhoverVFX(UWidget* TargetWidget)
{
	PlayUISound(ERetrieveUISoundEvent::Unhover);
	return PlayUIVFXOnWidget(RetrieveGameplayTags::UI_VFX_Button_Unhover, TargetWidget);
}

bool URetrieveUIVFXWidget::PlayButtonPressVFX(UWidget* TargetWidget)
{
	PlayUISound(ERetrieveUISoundEvent::Press);
	return PlayUIVFXOnWidget(RetrieveGameplayTags::UI_VFX_Button_Press, TargetWidget);
}

bool URetrieveUIVFXWidget::PlayButtonReleaseVFX(UWidget* TargetWidget)
{
	PlayUISound(ERetrieveUISoundEvent::Release);
	return PlayUIVFXOnWidget(RetrieveGameplayTags::UI_VFX_Button_Release, TargetWidget);
}

bool URetrieveUIVFXWidget::PlayTabSwitchVFX(UWidget* TargetWidget)
{
	PlayUISound(ERetrieveUISoundEvent::TabSwitch);
	return PlayUIVFXOnWidget(RetrieveGameplayTags::UI_VFX_Tab_Switch, TargetWidget);
}

UWidget* URetrieveUIVFXWidget::ResolveDefaultVFXTarget() const
{
	return DefaultVFXTarget ? DefaultVFXTarget.Get() : GetRootWidget();
}

void URetrieveUIVFXWidget::ApplyPresetAtAlpha(UWidget* TargetWidget, const FRetrieveUIVFXPreset& Preset, float Alpha) const
{
	if (!TargetWidget)
	{
		return;
	}

	FWidgetTransform Transform = TargetWidget->GetRenderTransform();
	Transform.Translation = FMath::Lerp(Preset.StartTranslation, Preset.EndTranslation, Alpha);
	Transform.Scale = FMath::Lerp(Preset.StartScale, Preset.EndScale, Alpha);
	TargetWidget->SetRenderTransform(Transform);
	TargetWidget->SetRenderOpacity(FMath::Clamp(FMath::Lerp(Preset.StartOpacity, Preset.EndOpacity, Alpha), 0.0f, 1.0f));
}

void URetrieveUIVFXWidget::PlayUISound(ERetrieveUISoundEvent Event) const
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const URetrieveUISoundSubsystem* SoundSubsystem = GameInstance->GetSubsystem<URetrieveUISoundSubsystem>())
		{
			SoundSubsystem->PlayUISound(this, Event);
		}
	}
}

void URetrieveUIVFXWidget::RegisterSoundButton(UButton* Button)
{
	if (!Button)
	{
		return;
	}

	for (const TObjectPtr<UObject>& BinderObject : SoundButtonBinders)
	{
		if (const URetrieveUISoundButtonBinder* Binder = Cast<URetrieveUISoundButtonBinder>(BinderObject))
		{
			if (Binder->IsBoundTo(Button))
			{
				return;
			}
		}
	}

	URetrieveUISoundButtonBinder* Binder = NewObject<URetrieveUISoundButtonBinder>(this);
	Binder->Bind(this, Button);
	SoundButtonBinders.Add(Binder);
}
