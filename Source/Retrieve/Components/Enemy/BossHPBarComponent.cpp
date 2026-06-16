#include "Components/Enemy/BossHPBarComponent.h"

#include "Components/Combat/RetrieveHealthComponent.h"
#include "Player/RetrievePlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"

UBossHPBarComponent::UBossHPBarComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBossHPBarComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearFromHUD();
	Super::EndPlay(EndPlayReason);
}

void UBossHPBarComponent::Show()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &ThisClass::TryBindToHUD);
	}
}

void UBossHPBarComponent::Hide()
{
	ClearFromHUD();
}

FText UBossHPBarComponent::GetDisplayName() const
{
	if (!DisplayName.IsEmpty())
	{
		return DisplayName;
	}
	return FText::FromString(GetOwner() ? GetOwner()->GetName() : TEXT("Boss"));
}

void UBossHPBarComponent::TryBindToHUD()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	URetrieveHealthComponent* HealthComp = Owner->FindComponentByClass<URetrieveHealthComponent>();
	if (!HealthComp)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ARetrievePlayerController* PC = Cast<ARetrievePlayerController>(World->GetFirstPlayerController());
	if (!PC || !PC->GetHUDViewModel())
	{
		if (++BindAttempts <= 30)
		{
			World->GetTimerManager().SetTimer(RetryTimer, this, &ThisClass::TryBindToHUD, 0.2f, false);
		}
		return;
	}

	BindAttempts = 0;
	World->GetTimerManager().ClearTimer(RetryTimer);
	PC->TryBindBossToHUD(HealthComp, GetDisplayName());
}

void UBossHPBarComponent::ClearFromHUD()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RetryTimer);

		if (ARetrievePlayerController* PC = Cast<ARetrievePlayerController>(World->GetFirstPlayerController()))
		{
			PC->TryBindBossToHUD(nullptr, FText::GetEmpty());
		}
	}

	BindAttempts = 0;
}
