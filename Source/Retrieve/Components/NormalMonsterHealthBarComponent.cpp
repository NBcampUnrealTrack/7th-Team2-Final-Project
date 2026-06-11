#include "Components/NormalMonsterHealthBarComponent.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"
#include "Components/RetrieveHealthComponent.h"
#include "TimerManager.h"
#include "UI/HUD/RetrieveNormalMonsterHealthBarWidget.h"

namespace
{
UProgressBar* FindHealthProgressBar(UUserWidget* Widget)
{
	if (!Widget || !Widget->WidgetTree)
	{
		return nullptr;
	}

	if (UProgressBar* NamedBar = Widget->WidgetTree->FindWidget<UProgressBar>(TEXT("HPBar")))
	{
		return NamedBar;
	}

	UProgressBar* FirstProgressBar = nullptr;
	Widget->WidgetTree->ForEachWidget([&FirstProgressBar](UWidget* ChildWidget)
	{
		if (!FirstProgressBar)
		{
			FirstProgressBar = Cast<UProgressBar>(ChildWidget);
		}
	});

	return FirstProgressBar;
}
}

UNormalMonsterHealthBarComponent::UNormalMonsterHealthBarComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	SetWidgetSpace(EWidgetSpace::Screen);
	SetDrawSize(FVector2D(120.f, 12.f));
	SetPivot(FVector2D(0.5f, 0.5f));
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetHiddenInGame(true);
	SetWidgetClass(URetrieveNormalMonsterHealthBarWidget::StaticClass());
}

void UNormalMonsterHealthBarComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetNetMode() == NM_DedicatedServer || !bHealthBarEnabled)
	{
		SetComponentTickEnabled(false);
		SetHiddenInGame(true);
		return;
	}

	BindToHealthComponent();
	RefreshHealthPercent();
	SetComponentTickEnabled(BoundHealthComponent != nullptr);

	if (bShowOnBeginPlayForDebug)
	{
		SetBarVisible(true);
	}
	else
	{
		HideBar();
	}
}

void UNormalMonsterHealthBarComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimerHandle);
	}

	UnbindFromHealthComponent();
	Super::EndPlay(EndPlayReason);
}

void UNormalMonsterHealthBarComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bHealthBarEnabled || !BoundHealthComponent)
	{
		return;
	}

	const float CurrentHealth = BoundHealthComponent->GetHealth();
	const float CurrentMaxHealth = BoundHealthComponent->GetMaxHealth();
	const bool bHealthChanged = !FMath::IsNearlyEqual(CurrentHealth, LastObservedHealth);
	const bool bMaxHealthChanged = !FMath::IsNearlyEqual(CurrentMaxHealth, LastObservedMaxHealth);

	if (!bHealthChanged && !bMaxHealthChanged)
	{
		return;
	}

	const bool bTookDamage = LastObservedHealth >= 0.f && CurrentHealth < LastObservedHealth;
	LastObservedHealth = CurrentHealth;
	LastObservedMaxHealth = CurrentMaxHealth;
	RefreshHealthPercent();

	if (bShowOnBeginPlayForDebug)
	{
		SetBarVisible(true);
		return;
	}

	if (bTookDamage && ShouldShowForHealth(CurrentHealth, CurrentMaxHealth))
	{
		ShowForDuration();
	}
	else if (!ShouldShowForHealth(CurrentHealth, CurrentMaxHealth))
	{
		HideBar();
	}
}

void UNormalMonsterHealthBarComponent::SetHealthBarEnabled(bool bNewEnabled)
{
	bHealthBarEnabled = bNewEnabled;

	if (!bHealthBarEnabled)
	{
		HideBar();
		UnbindFromHealthComponent();
	}
	else if (HasBegunPlay())
	{
		BindToHealthComponent();
		RefreshHealthPercent();
		SetComponentTickEnabled(BoundHealthComponent != nullptr);
	}
}

void UNormalMonsterHealthBarComponent::BindToHealthComponent()
{
	if (BoundHealthComponent)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	BoundHealthComponent = Owner->FindComponentByClass<URetrieveHealthComponent>();
	if (!BoundHealthComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: NormalMonsterHealthBarComponent could not find RetrieveHealthComponent."),
		       *GetNameSafe(Owner));
		return;
	}

	BoundHealthComponent->OnHealthChanged.AddDynamic(this, &ThisClass::HandleHealthChanged);
	BoundHealthComponent->OnMaxHealthChanged.AddDynamic(this, &ThisClass::HandleMaxHealthChanged);
	BoundHealthComponent->OnDeathStarted.AddDynamic(this, &ThisClass::HandleDeathStarted);

	LastObservedHealth = BoundHealthComponent->GetHealth();
	LastObservedMaxHealth = BoundHealthComponent->GetMaxHealth();
}

void UNormalMonsterHealthBarComponent::UnbindFromHealthComponent()
{
	if (BoundHealthComponent)
	{
		BoundHealthComponent->OnHealthChanged.RemoveDynamic(this, &ThisClass::HandleHealthChanged);
		BoundHealthComponent->OnMaxHealthChanged.RemoveDynamic(this, &ThisClass::HandleMaxHealthChanged);
		BoundHealthComponent->OnDeathStarted.RemoveDynamic(this, &ThisClass::HandleDeathStarted);
		BoundHealthComponent = nullptr;
	}

	LastObservedHealth = -1.f;
	LastObservedMaxHealth = -1.f;
	SetComponentTickEnabled(false);
}

void UNormalMonsterHealthBarComponent::HandleHealthChanged(float NewHealth)
{
	RefreshHealthPercent();

	if (!bHealthBarEnabled || !BoundHealthComponent)
	{
		return;
	}

	const float MaxHealth = BoundHealthComponent->GetMaxHealth();
	LastObservedHealth = NewHealth;
	LastObservedMaxHealth = MaxHealth;

	if (ShouldShowForHealth(NewHealth, MaxHealth))
	{
		ShowForDuration();
	}
	else
	{
		HideBar();
	}
}

void UNormalMonsterHealthBarComponent::HandleMaxHealthChanged(float NewMaxHealth)
{
	RefreshHealthPercent();

	LastObservedMaxHealth = NewMaxHealth;
	if (BoundHealthComponent)
	{
		LastObservedHealth = BoundHealthComponent->GetHealth();
	}

	if (!BoundHealthComponent || !ShouldShowForHealth(BoundHealthComponent->GetHealth(), NewMaxHealth))
	{
		HideBar();
	}
}

void UNormalMonsterHealthBarComponent::HandleDeathStarted(AActor* OwningActor)
{
	HideBar();
}

void UNormalMonsterHealthBarComponent::RefreshHealthPercent()
{
	if (!BoundHealthComponent)
	{
		return;
	}

	const float MaxHealth = BoundHealthComponent->GetMaxHealth();
	const float Percent = MaxHealth > 0.f
		? FMath::Clamp(BoundHealthComponent->GetHealth() / MaxHealth, 0.f, 1.f)
		: 0.f;

	if (URetrieveNormalMonsterHealthBarWidget* HealthBarWidget =
		Cast<URetrieveNormalMonsterHealthBarWidget>(GetUserWidgetObject()))
	{
		HealthBarWidget->SetHealthPercent(Percent);
		return;
	}

	if (UProgressBar* HealthProgressBar = FindHealthProgressBar(GetUserWidgetObject()))
	{
		HealthProgressBar->SetPercent(Percent);
	}
}

void UNormalMonsterHealthBarComponent::ShowForDuration()
{
	SetBarVisible(true);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimerHandle);
		World->GetTimerManager().SetTimer(
			HideTimerHandle,
			this,
			&ThisClass::HideBar,
			VisibleDurationAfterDamage,
			false);
	}
}

void UNormalMonsterHealthBarComponent::HideBar()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HideTimerHandle);
	}

	SetBarVisible(false);
}

void UNormalMonsterHealthBarComponent::SetBarVisible(bool bNewVisible)
{
	SetHiddenInGame(!bNewVisible);
	SetVisibility(bNewVisible, true);

	if (URetrieveNormalMonsterHealthBarWidget* HealthBarWidget =
		Cast<URetrieveNormalMonsterHealthBarWidget>(GetUserWidgetObject()))
	{
		if (bNewVisible)
		{
			HealthBarWidget->PlayShowAnimation();
		}
		else
		{
			HealthBarWidget->PlayHideAnimation();
		}
	}
}

bool UNormalMonsterHealthBarComponent::ShouldShowForHealth(float Health, float MaxHealth) const
{
	if (Health <= 0.f || MaxHealth <= 0.f)
	{
		return false;
	}

	return !bHideWhenFullHealth || Health < MaxHealth;
}
