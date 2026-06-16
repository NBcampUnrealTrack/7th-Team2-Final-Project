#include "Components/Enemy/NormalMonsterHealthBarComponent.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ProgressBar.h"
#include "Components/Combat/RetrieveHealthComponent.h"
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
	SetDrawSize(FVector2D(120.f, 16.f));
	SetPivot(FVector2D(0.5f, 0.5f));
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);
	SetHiddenInGame(true);

	// WBP Blueprint 클래스를 우선 로드 (Text_MonsterName / Text_HPValue 바인딩 보장)
	static ConstructorHelpers::FClassFinder<UUserWidget> HealthBarWBP(
		TEXT("/Game/Retrieve/UI/WBP_NormalMonsterHealthBar"));
	if (HealthBarWBP.Succeeded())
	{
		WBPWidgetClass = HealthBarWBP.Class;
		SetWidgetClass(HealthBarWBP.Class);
	}
	else
	{
		SetWidgetClass(URetrieveNormalMonsterHealthBarWidget::StaticClass());
	}
}

void UNormalMonsterHealthBarComponent::OnRegister()
{
	// Blueprint 직렬화 이후 Widget Class가 C++ 기본 클래스로 덮어쓰여진 경우 WBP로 강제 복원.
	// OnRegister에서 처리해야 InitWidget() 호출 시점과 무관하게 올바른 클래스가 사용됨.
	if (!GetWidgetClass() || GetWidgetClass() == URetrieveNormalMonsterHealthBarWidget::StaticClass())
	{
		UClass* TargetClass = WBPWidgetClass;
		if (!TargetClass)
		{
			// FClassFinder 실패 시 직접 로드 (이미 메모리에 있으면 즉시 반환)
			TargetClass = LoadClass<UUserWidget>(nullptr,
				TEXT("/Game/Retrieve/UI/WBP_NormalMonsterHealthBar.WBP_NormalMonsterHealthBar_C"));
		}
		if (TargetClass)
		{
			UE_LOG(LogTemp, Warning, TEXT("[MonsterHPBar] %s: Widget Class를 %s로 강제 복원"),
				*GetNameSafe(GetOwner()), *TargetClass->GetName());
			SetWidgetClass(TargetClass);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("[MonsterHPBar] WBP_NormalMonsterHealthBar 클래스를 찾을 수 없음. 경로 확인 필요."));
		}
	}

	Super::OnRegister();
}

void UNormalMonsterHealthBarComponent::BeginPlay()
{
	Super::BeginPlay();  // UWidgetComponent::BeginPlay() 내부에서 InitWidget() 호출

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

	// 이름 / 등급 정보를 위젯에 초기 전달 — DisplayName이 비어있어도 MonsterTypeTag가 설정된 경우 항상 적용
	if (URetrieveNormalMonsterHealthBarWidget* HealthBarWidget =
		Cast<URetrieveNormalMonsterHealthBarWidget>(GetUserWidgetObject()))
	{
		HealthBarWidget->SetMonsterInfo(
			MonsterDisplayName, MonsterTypeTag,
			LastObservedHealth, LastObservedMaxHealth);
	}
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

void UNormalMonsterHealthBarComponent::SetMonsterIdentity(FText InDisplayName, FGameplayTag InTypeTag)
{
	MonsterDisplayName = InDisplayName;
	MonsterTypeTag = InTypeTag;

	if (URetrieveNormalMonsterHealthBarWidget* HealthBarWidget =
		Cast<URetrieveNormalMonsterHealthBarWidget>(GetUserWidgetObject()))
	{
		const float CurrentHP = BoundHealthComponent ? BoundHealthComponent->GetHealth() : 0.f;
		const float MaxHP = BoundHealthComponent ? BoundHealthComponent->GetMaxHealth() : 0.f;
		HealthBarWidget->SetMonsterInfo(MonsterDisplayName, MonsterTypeTag, CurrentHP, MaxHP);
	}
}

void UNormalMonsterHealthBarComponent::RefreshHealthPercent()
{
	if (!BoundHealthComponent)
	{
		return;
	}

	const float CurrentHP = BoundHealthComponent->GetHealth();
	const float MaxHP = BoundHealthComponent->GetMaxHealth();
	const float Percent = MaxHP > 0.f
		? FMath::Clamp(CurrentHP / MaxHP, 0.f, 1.f)
		: 0.f;

	if (URetrieveNormalMonsterHealthBarWidget* HealthBarWidget =
		Cast<URetrieveNormalMonsterHealthBarWidget>(GetUserWidgetObject()))
	{
		HealthBarWidget->SetHealthPercent(Percent);
		HealthBarWidget->SetHPValue(CurrentHP, MaxHP);
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
