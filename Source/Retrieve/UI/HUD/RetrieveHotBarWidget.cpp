#include "UI/HUD/RetrieveHotBarWidget.h"

#include "Components/ProgressBar.h"
#include "Components/Combat/RetrieveHealthComponent.h"
#include "Components/Player/StaminaComponent.h"
#include "GameFramework/Pawn.h"
#include "UObject/UnrealType.h"

void URetrieveHotBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	HealthBarWidget = Cast<UUserWidget>(GetWidgetFromName(TEXT("HUD_HPBar")));
	StaminaBarWidget = Cast<UUserWidget>(GetWidgetFromName(TEXT("HUD_MPBar")));
	HealthContainerWidget = Cast<UUserWidget>(GetWidgetFromName(TEXT("HUD_HPContainer")));
	StaminaContainerWidget = Cast<UUserWidget>(GetWidgetFromName(TEXT("HUD_MPContainer")));

	TryBindToOwningPawn();
	RefreshAllBars();
}

void URetrieveHotBarWidget::NativeDestruct()
{
	UnbindFromComponents();
	HealthBarWidget = nullptr;
	StaminaBarWidget = nullptr;
	HealthContainerWidget = nullptr;
	StaminaContainerWidget = nullptr;

	Super::NativeDestruct();
}

void URetrieveHotBarWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// The HUD can be constructed before possession or component ASC setup.
	// Re-resolve only while unbound or when the owning pawn changes.
	if (!BoundPawn.IsValid() || BoundPawn.Get() != GetOwningPlayerPawn() || !HealthComponent || !StaminaComponent)
	{
		TryBindToOwningPawn();
	}
}

void URetrieveHotBarWidget::TryBindToOwningPawn()
{
	APawn* OwningPawn = GetOwningPlayerPawn();
	if (!OwningPawn)
	{
		return;
	}

	URetrieveHealthComponent* NewHealthComponent = OwningPawn->FindComponentByClass<URetrieveHealthComponent>();
	UStaminaComponent* NewStaminaComponent = OwningPawn->FindComponentByClass<UStaminaComponent>();

	if (BoundPawn.Get() == OwningPawn && HealthComponent == NewHealthComponent && StaminaComponent == NewStaminaComponent)
	{
		return;
	}

	UnbindFromComponents();
	BoundPawn = OwningPawn;
	HealthComponent = NewHealthComponent;
	StaminaComponent = NewStaminaComponent;

	if (HealthComponent)
	{
		HealthComponent->OnHealthChanged.AddDynamic(this, &ThisClass::HandleHealthChanged);
		HealthComponent->OnMaxHealthChanged.AddDynamic(this, &ThisClass::HandleMaxHealthChanged);
	}

	if (StaminaComponent)
	{
		StaminaComponent->OnStaminaChanged.AddDynamic(this, &ThisClass::HandleStaminaChanged);
	}

	RefreshAllBars();
}

void URetrieveHotBarWidget::UnbindFromComponents()
{
	if (HealthComponent)
	{
		HealthComponent->OnHealthChanged.RemoveDynamic(this, &ThisClass::HandleHealthChanged);
		HealthComponent->OnMaxHealthChanged.RemoveDynamic(this, &ThisClass::HandleMaxHealthChanged);
	}

	if (StaminaComponent)
	{
		StaminaComponent->OnStaminaChanged.RemoveDynamic(this, &ThisClass::HandleStaminaChanged);
	}

	HealthComponent = nullptr;
	StaminaComponent = nullptr;
	BoundPawn.Reset();
}

void URetrieveHotBarWidget::RefreshAllBars()
{
	RefreshHealthBar();
	RefreshStaminaBar();
}

void URetrieveHotBarWidget::RefreshHealthBar()
{
	if (HealthComponent)
	{
		ApplyHealth(HealthComponent->GetHealth(), HealthComponent->GetMaxHealth());
	}
}

void URetrieveHotBarWidget::RefreshStaminaBar()
{
	if (StaminaComponent)
	{
		ApplyStamina(StaminaComponent->GetStamina(), StaminaComponent->GetMaxStamina());
	}
}

void URetrieveHotBarWidget::ApplyHealth(float Current, float Max)
{
	UpdateFantasyBar(HealthBarWidget, Current, Max);
	const float Fraction = Max > 0.f ? Current / Max : 0.f;
	SetProgressBarPercent(HealthContainerWidget, TEXT("BAR_Health"), Fraction);
}

void URetrieveHotBarWidget::ApplyStamina(float Current, float Max)
{
	UpdateFantasyBar(StaminaBarWidget, Current, Max);
	const float Fraction = Max > 0.f ? Current / Max : 0.f;
	SetProgressBarPercent(StaminaContainerWidget, TEXT("BAR_Health"), Fraction);
}

void URetrieveHotBarWidget::HandleHealthChanged(float NewHealth)
{
	ApplyHealth(NewHealth, HealthComponent ? HealthComponent->GetMaxHealth() : 0.f);
}

void URetrieveHotBarWidget::HandleMaxHealthChanged(float NewMaxHealth)
{
	ApplyHealth(HealthComponent ? HealthComponent->GetHealth() : 0.f, NewMaxHealth);
}

void URetrieveHotBarWidget::HandleStaminaChanged(float NewStamina, float MaxStamina)
{
	ApplyStamina(NewStamina, MaxStamina);
}

void URetrieveHotBarWidget::UpdateFantasyBar(UUserWidget* BarWidget, float CurrentValue, float MaxValue)
{
	if (!BarWidget)
	{
		return;
	}

	const int32 SafeMax = FMath::Max(0, FMath::RoundToInt(MaxValue));
	const int32 SafeCurrent = FMath::Clamp(FMath::RoundToInt(CurrentValue), 0, SafeMax);

	SetIntegerProperty(BarWidget, TEXT("CurrentXP"), SafeCurrent);
	SetIntegerProperty(BarWidget, TEXT("MaxXP"), SafeMax);
	CallNoArgFunction(BarWidget, TEXT("UpdateWidget"));
}

bool URetrieveHotBarWidget::SetIntegerProperty(UObject* Object, FName PropertyName, int32 Value)
{
	if (!Object)
	{
		return false;
	}

	if (FIntProperty* Property = FindFProperty<FIntProperty>(Object->GetClass(), PropertyName))
	{
		Property->SetPropertyValue_InContainer(Object, Value);
		return true;
	}

	return false;
}

bool URetrieveHotBarWidget::CallNoArgFunction(UObject* Object, FName FunctionName)
{
	if (!Object)
	{
		return false;
	}

	if (UFunction* Function = Object->FindFunction(FunctionName))
	{
		Object->ProcessEvent(Function, nullptr);
		return true;
	}

	return false;
}

bool URetrieveHotBarWidget::SetProgressBarPercent(UUserWidget* Container, FName ProgressBarName, float Percent)
{
	if (!Container)
	{
		return false;
	}
	if (UProgressBar* Bar = Cast<UProgressBar>(Container->GetWidgetFromName(ProgressBarName)))
	{
		Bar->SetPercent(FMath::Clamp(Percent, 0.f, 1.f));
		return true;
	}
	return false;
}
