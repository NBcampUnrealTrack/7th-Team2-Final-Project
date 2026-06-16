#include "UI/ViewModels/BossStatusViewModel.h"

#include "Components/Combat/RetrieveHealthComponent.h"

FText UBossStatusViewModel::GetHealthText() const
{
	return FText::FromString(FString::Printf(
		TEXT("%d / %d"),
		FMath::RoundToInt(CurrentHealth),
		FMath::RoundToInt(MaxHealth)));
}

void UBossStatusViewModel::BindToBoss(URetrieveHealthComponent* InHealth, FText InBossName)
{
	UnbindFromBoss();
	if (!InHealth)
	{
		return;
	}

	BoundHealth = InHealth;
	InHealth->OnHealthChanged.AddDynamic(this, &ThisClass::HandleHealthChanged);
	InHealth->OnMaxHealthChanged.AddDynamic(this, &ThisClass::HandleMaxHealthChanged);
	InHealth->OnDeathStarted.AddDynamic(this, &ThisClass::HandleDeathStarted);

	BossName = InBossName;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetBossName);

	HandleMaxHealthChanged(InHealth->GetMaxHealth());
	HandleHealthChanged(InHealth->GetHealth());

	DisplayedHealthFraction = GetHealthFraction();
	InterpTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::TickInterp));

	bVisible = true;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetIsVisible);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetSlateVisibility);
}

void UBossStatusViewModel::UnbindFromBoss()
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
		Health->OnDeathStarted.RemoveDynamic(this, &ThisClass::HandleDeathStarted);
	}

	BoundHealth = nullptr;

	if (bVisible)
	{
		bVisible = false;
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetIsVisible);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetSlateVisibility);
	}
}

void UBossStatusViewModel::HandleHealthChanged(float NewHealth)
{
	if (CurrentHealth == NewHealth)
	{
		return;
	}

	CurrentHealth = NewHealth;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetCurrentHealth);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetHealthFraction);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetHealthText);
}

void UBossStatusViewModel::HandleMaxHealthChanged(float NewMaxHealth)
{
	if (MaxHealth == NewMaxHealth)
	{
		return;
	}

	MaxHealth = NewMaxHealth;
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetMaxHealth);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetHealthFraction);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetHealthText);
}

void UBossStatusViewModel::HandleDeathStarted(AActor* OwningActor)
{
	UnbindFromBoss();
}

bool UBossStatusViewModel::TickInterp(float DeltaTime)
{
	const float Target = GetHealthFraction();
	if (!FMath::IsNearlyEqual(DisplayedHealthFraction, Target, 0.0005f))
	{
		DisplayedHealthFraction = FMath::FInterpTo(DisplayedHealthFraction, Target, DeltaTime, HealthInterpSpeed);
		UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetDisplayedHealthFraction);
	}
	return true;
}
