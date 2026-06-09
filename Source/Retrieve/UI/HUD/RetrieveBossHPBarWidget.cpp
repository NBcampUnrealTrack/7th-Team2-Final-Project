#include "UI/HUD/RetrieveBossHPBarWidget.h"

#include "INotifyFieldValueChanged.h"
#include "MVVMSubsystem.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Player/RetrievePlayerController.h"
#include "UI/ViewModels/BossStatusViewModel.h"
#include "UI/ViewModels/HUDViewModel.h"
#include "UObject/UnrealType.h"
#include "View/MVVMView.h"

bool URetrieveBossHPBarWidget::SetNumericWidgetProperty(UObject* Object, FName PropertyName, float Value)
{
	if (!Object)
	{
		return false;
	}

	FProperty* Property = Object->GetClass()->FindPropertyByName(PropertyName);
	if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
	{
		FloatProperty->SetPropertyValue_InContainer(Object, Value);
		return true;
	}
	if (FDoubleProperty* DoubleProperty = CastField<FDoubleProperty>(Property))
	{
		DoubleProperty->SetPropertyValue_InContainer(Object, Value);
		return true;
	}

	return false;
}

bool URetrieveBossHPBarWidget::CallFloatWidgetFunction(UObject* Object, FName FunctionName, float Value)
{
	if (!Object)
	{
		return false;
	}

	UFunction* Function = Object->FindFunction(FunctionName);
	if (!Function)
	{
		return false;
	}

	TArray<uint8> Params;
	Params.SetNumZeroed(Function->ParmsSize);

	bool bWroteParameter = false;
	for (TFieldIterator<FProperty> It(Function); It; ++It)
	{
		FProperty* Property = *It;
		if (!Property || !Property->HasAnyPropertyFlags(CPF_Parm) || Property->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			continue;
		}

		void* ParamValue = Property->ContainerPtrToValuePtr<void>(Params.GetData());
		if (FFloatProperty* FloatProperty = CastField<FFloatProperty>(Property))
		{
			FloatProperty->SetPropertyValue(ParamValue, Value);
			bWroteParameter = true;
			break;
		}
		if (FDoubleProperty* DoubleProperty = CastField<FDoubleProperty>(Property))
		{
			DoubleProperty->SetPropertyValue(ParamValue, Value);
			bWroteParameter = true;
			break;
		}
	}

	if (!bWroteParameter)
	{
		return false;
	}

	Object->ProcessEvent(Function, Params.GetData());
	return true;
}

void URetrieveBossHPBarWidget::AddBossFieldHandle(
	UE::FieldNotification::FFieldId FieldId,
	const INotifyFieldValueChanged::FFieldValueChangedDelegate& Delegate)
{
	if (!BossStatusViewModel || !FieldId.IsValid())
	{
		return;
	}

	if (INotifyFieldValueChanged* Notify = Cast<INotifyFieldValueChanged>(BossStatusViewModel))
	{
		BossFieldHandles.Add(FieldId.GetName(), Notify->AddFieldValueChangedDelegate(FieldId, Delegate));
	}
}

void URetrieveBossHPBarWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	UpdateViewportScale();

	if (!IsDesignTime() && !BossStatusViewModel)
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void URetrieveBossHPBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UpdateViewportScale();
	SetVisibility(ESlateVisibility::Collapsed);
	BindToBossStatusViewModel();
	RefreshFromViewModel();
}

void URetrieveBossHPBarWidget::NativeDestruct()
{
	UnbindFromBossStatusViewModel();

	Super::NativeDestruct();
}

void URetrieveBossHPBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	UpdateViewportScale();
}

void URetrieveBossHPBarWidget::SetBossStatusViewModel(UBossStatusViewModel* InViewModel)
{
	BindToBossStatusViewModel(InViewModel);
	RefreshFromViewModel();
}

void URetrieveBossHPBarWidget::BindToBossStatusViewModel()
{
	ARetrievePlayerController* RetrievePC = Cast<ARetrievePlayerController>(GetOwningPlayer());
	if (!RetrievePC)
	{
		return;
	}

	UHUDViewModel* HUDViewModel = RetrievePC->GetHUDViewModel();
	if (!HUDViewModel)
	{
		return;
	}

	BindToBossStatusViewModel(HUDViewModel->GetBossStatus());
}

void URetrieveBossHPBarWidget::BindToBossStatusViewModel(UBossStatusViewModel* InViewModel)
{
	if (BossStatusViewModel == InViewModel && BossStatusViewModel)
	{
		return;
	}

	UnbindFromBossStatusViewModel();

	BossStatusViewModel = InViewModel;
	if (!BossStatusViewModel)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	if (UMVVMSubsystem* MVVM = GEngine ? GEngine->GetEngineSubsystem<UMVVMSubsystem>() : nullptr)
	{
		if (UMVVMView* View = MVVM->GetViewFromUserWidget(this))
		{
			View->SetViewModel(TEXT("BossStatus"), BossStatusViewModel);
			View->SetViewModel(TEXT("BossStatusViewModel"), BossStatusViewModel);
		}
	}

	const INotifyFieldValueChanged::FFieldValueChangedDelegate Delegate =
		INotifyFieldValueChanged::FFieldValueChangedDelegate::CreateUObject(
			this,
			&ThisClass::HandleBossFieldChanged);

	AddBossFieldHandle(UBossStatusViewModel::FFieldNotificationClassDescriptor::GetIsVisible, Delegate);
	AddBossFieldHandle(UBossStatusViewModel::FFieldNotificationClassDescriptor::GetBossName, Delegate);
	AddBossFieldHandle(UBossStatusViewModel::FFieldNotificationClassDescriptor::GetHealthText, Delegate);
	AddBossFieldHandle(UBossStatusViewModel::FFieldNotificationClassDescriptor::GetDisplayedHealthFraction, Delegate);
}

void URetrieveBossHPBarWidget::UnbindFromBossStatusViewModel()
{
	if (BossStatusViewModel)
	{
		if (INotifyFieldValueChanged* Notify = Cast<INotifyFieldValueChanged>(BossStatusViewModel))
		{
			for (const TPair<FName, FDelegateHandle>& Pair : BossFieldHandles)
			{
				Notify->RemoveFieldValueChangedDelegate(UE::FieldNotification::FFieldId(Pair.Key, INDEX_NONE), Pair.Value);
			}
		}
	}

	BossFieldHandles.Reset();
	BossStatusViewModel = nullptr;
}

void URetrieveBossHPBarWidget::RefreshFromViewModel()
{
	if (!BossStatusViewModel)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const bool bVisible = BossStatusViewModel->GetIsVisible();
	SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	if (!bVisible)
	{
		return;
	}

	if (TXT_Name)
	{
		TXT_Name->SetText(BossStatusViewModel->GetBossName());
	}

	if (TXT_HP)
	{
		TXT_HP->SetText(BossStatusViewModel->GetHealthText());
	}

	SetFantasyHealthBarPercent(BossStatusViewModel->GetDisplayedHealthFraction());
}

void URetrieveBossHPBarWidget::HandleBossFieldChanged(UObject* /*Object*/, UE::FieldNotification::FFieldId /*FieldId*/)
{
	RefreshFromViewModel();
}

void URetrieveBossHPBarWidget::SetFantasyHealthBarPercent(float Percent)
{
	const float ClampedPercent = FMath::Clamp(Percent, 0.f, 1.f);

	if (HUD_HealthBar_Enemy)
	{
		HUD_HealthBar_Enemy->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		SetNumericWidgetProperty(HUD_HealthBar_Enemy, TEXT("FillAmount"), ClampedPercent);
		CallFloatWidgetFunction(HUD_HealthBar_Enemy, TEXT("SetFillAmount"), ClampedPercent);
		HUD_HealthBar_Enemy->InvalidateLayoutAndVolatility();
	}

	if (UProgressBar* ProgressBar = Cast<UProgressBar>(GetWidgetFromName(TEXT("PB_BossHealth"))))
	{
		ProgressBar->SetVisibility(HUD_HealthBar_Enemy ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
		ProgressBar->SetPercent(ClampedPercent);
	}
}

void URetrieveBossHPBarWidget::UpdateViewportScale()
{
	if (!bAutoScaleWithViewport || DesignViewportSize.X <= 0.f || DesignViewportSize.Y <= 0.f)
	{
		if (!FMath::IsNearlyEqual(LastAppliedViewportScale, 1.f))
		{
			SetRenderScale(FVector2D(1.f, 1.f));
			LastAppliedViewportScale = 1.f;
		}
		return;
	}

	const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this);
	if (ViewportSize.X <= 0.f || ViewportSize.Y <= 0.f)
	{
		return;
	}

	const float WidthScale = ViewportSize.X / DesignViewportSize.X;
	const float HeightScale = ViewportSize.Y / DesignViewportSize.Y;
	const float TargetScale = FMath::Clamp(FMath::Min(WidthScale, HeightScale), MinViewportScale, MaxViewportScale);

	if (!FMath::IsNearlyEqual(LastAppliedViewportScale, TargetScale, 0.001f))
	{
		SetRenderTransformPivot(FVector2D(0.5f, 0.f));
		SetRenderScale(FVector2D(TargetScale, TargetScale));
		LastAppliedViewportScale = TargetScale;
	}
}
