#include "UI/ViewModels/PlayerStatusViewModel.h"

#include "Components/RetrieveHealthComponent.h"

void UPlayerStatusViewModel::BindToHealth(URetrieveHealthComponent* InHealth)
{
	UnbindFromHealth();
	if (!InHealth)
	{
		return;
	}
	
	BoundHealth = InHealth;
	InHealth->OnHealthChanged.AddDynamic(this, &ThisClass::HandleHealthChanged);
	InHealth->OnMaxHealthChanged.AddDynamic(this, &ThisClass::HandleMaxHealthChanged);
	
	HandleMaxHealthChanged(InHealth->GetMaxHealth());
	HandleHealthChanged(InHealth->GetHealth());
	
	InterpTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickInterp));
	DisplayedHealthFraction = GetHealthFraction();
}

void UPlayerStatusViewModel::UnbindFromHealth()
{
	if (InterpTicker.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(InterpTicker);
		InterpTicker.Reset();
	}
	
	if (URetrieveHealthComponent* Health = BoundHealth.Get())
	{
		Health->OnHealthChanged.RemoveDynamic(this, &ThisClass::HandleHealthChanged);
		Health->OnMaxHealthChanged.RemoveDynamic(this, &ThisClass::HandleMaxHealthChanged);
	}
	
	BoundHealth = nullptr;
}

void UPlayerStatusViewModel::HandleHealthChanged(float NewHealth)
{
	if (CurrentHealth == NewHealth)
	{
		return;
	}
	
	CurrentHealth = NewHealth;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetCurrentHealth);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetHealthFraction);
}

void UPlayerStatusViewModel::HandleMaxHealthChanged(float NewMaxHealth)
{
	if (MaxHealth == NewMaxHealth)
	{
		return;
	}
	
	MaxHealth = NewMaxHealth;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetMaxHealth);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetHealthFraction);
}

bool UPlayerStatusViewModel::TickInterp(float DeltaTime)
{
	const float Target = GetHealthFraction();
	if (!FMath::IsNearlyEqual(DisplayedHealthFraction, Target, 0.0005f))
	{
		DisplayedHealthFraction = FMath::FInterpTo(DisplayedHealthFraction, Target, DeltaTime, HealthInterpSpeed);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetDisplayedHealthFraction);
	}
	return true;
}
