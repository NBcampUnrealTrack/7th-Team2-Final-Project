#include "Components/Player/StaminaComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/CombatAttributeSet.h"
#include "Audio/RetrieveMusicSubsystem.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Settings/RetrieveStaminaSettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

namespace
{
	// 자연 회복 틱 주기(초). 실제 회복량 = RegenPerSecond × 이 값. 값 자체는 밸런스가 아니라 갱신 granularity.
	constexpr float StaminaRegenTickInterval = 0.1f;
}

UStaminaComponent::UStaminaComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UStaminaComponent::BeginPlay()
{
	Super::BeginPlay();
	ResolveASC();
}

void UStaminaComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UninitializeFromAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

void UStaminaComponent::InitializeWithAbilitySystem(UAbilitySystemComponent* InASC)
{
	if (AbilitySystemComponent == InASC && AttributeSet)
	{
		return;
	}

	UninitializeFromAbilitySystem();
	AbilitySystemComponent = InASC;
	TryBindAttributeSet();
}

void UStaminaComponent::UninitializeFromAbilitySystem()
{
	if (AbilitySystemComponent)
	{
		if (StaminaChangedHandle.IsValid())
		{
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
				UCombatAttributeSet::GetStaminaAttribute()).Remove(StaminaChangedHandle);
		}

		if (MaxStaminaChangedHandle.IsValid())
		{
			AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
				UCombatAttributeSet::GetMaxStaminaAttribute()).Remove(MaxStaminaChangedHandle);
		}
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RegenTickTimerHandle);
	}

	StaminaChangedHandle.Reset();
	MaxStaminaChangedHandle.Reset();
	AttributeSet = nullptr;
	AbilitySystemComponent = nullptr;
}

UAbilitySystemComponent* UStaminaComponent::ResolveASC()
{
	if (AbilitySystemComponent)
	{
		TryBindAttributeSet();
		return AbilitySystemComponent;
	}

	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
	if (ASC)
	{
		InitializeWithAbilitySystem(ASC);
	}
	return ASC;
}

bool UStaminaComponent::TryBindAttributeSet()
{
	if (!AbilitySystemComponent || AttributeSet)
	{
		return AttributeSet != nullptr;
	}

	AttributeSet = AbilitySystemComponent->GetSet<UCombatAttributeSet>();
	if (!AttributeSet)
	{
		return false;
	}

	StaminaChangedHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		                                             UCombatAttributeSet::GetStaminaAttribute())
	                                             .AddUObject(this, &UStaminaComponent::HandleStaminaAttributeChanged);

	MaxStaminaChangedHandle = AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		                                                UCombatAttributeSet::GetMaxStaminaAttribute())
	                                                .AddUObject(
		                                                this, &UStaminaComponent::HandleMaxStaminaAttributeChanged);

	InitStaminaPool();

	// 자연 회복은 권한 측에서 주기 틱으로만 처리(클라는 복제된 값 표시).
	if (const AActor* Owner = GetOwner(); Owner && Owner->HasAuthority())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				RegenTickTimerHandle, this, &UStaminaComponent::HandleStaminaTick, StaminaRegenTickInterval, /*bLoop=*/true);
		}
	}

	BroadcastStaminaChanged();
	return true;
}

void UStaminaComponent::HandleStaminaAttributeChanged(const FOnAttributeChangeData& Data)
{
	OnStaminaChanged.Broadcast(Data.NewValue, GetMaxStamina());

	// 소모(감소)를 감지하면 회복 지연 타이머를 리셋한다(회복에 의한 증가는 무시).
	// 가드 홀드 중 지속 드레인도 매 틱 감소라 이 경로로 회복이 계속 미뤄진다.
	if (Data.NewValue < Data.OldValue - KINDA_SMALL_NUMBER)
	{
		if (const UWorld* World = GetWorld())
		{
			LastSpendTimeSeconds = World->GetTimeSeconds();
		}
	}
}

void UStaminaComponent::InitStaminaPool()
{
	const AActor* Owner = GetOwner();
	if (!AbilitySystemComponent || !Owner || !Owner->HasAuthority())
	{
		return;
	}

	const float Max = GetDefault<URetrieveStaminaSettings>()->MaxStamina;
	AbilitySystemComponent->SetNumericAttributeBase(UCombatAttributeSet::GetMaxStaminaAttribute(), Max);
	AbilitySystemComponent->SetNumericAttributeBase(UCombatAttributeSet::GetStaminaAttribute(), Max);
}

void UStaminaComponent::HandleStaminaTick()
{
	const AActor* Owner = GetOwner();
	if (!AbilitySystemComponent || !AttributeSet || !Owner || !Owner->HasAuthority())
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const URetrieveStaminaSettings* Settings = GetDefault<URetrieveStaminaSettings>();

	// '전투 중'은 발검 스탠스 태그가 아니라 실제 교전(IsCombatActive)으로 판정 — 제자리 공격만으론 전투로 안 침.
	const URetrieveMusicSubsystem* MusicSubsystem = World->GetSubsystem<URetrieveMusicSubsystem>();
	const bool bInCombat = MusicSubsystem && MusicSubsystem->IsCombatActive();

	// 전투 중 질주는 스태미너를 소모(비전투는 무료). 소진 시 질주 강제 종료(재입력 필요).
	if (Settings->SprintDrainPerSecond > 0.f
		&& bInCombat
		&& AbilitySystemComponent->HasMatchingGameplayTag(RetrieveGameplayTags::State_Player_Sprinting))
	{
		const float Cur = GetStamina();
		const float Next = FMath::Max(0.f, Cur - Settings->SprintDrainPerSecond * StaminaRegenTickInterval);
		if (Next < Cur)
		{
			AbilitySystemComponent->SetNumericAttributeBase(UCombatAttributeSet::GetStaminaAttribute(), Next);
		}
		if (Next <= 0.f)
		{
			AbilitySystemComponent->SetLooseGameplayTagCount(RetrieveGameplayTags::State_Player_Sprinting, 0);
		}
		return;
	}

	// 소모 후 회복 지연은 전투 중에만 적용한다(비전투는 지연 없이 즉시 회복 재개).
	const float RegenDelay = bInCombat ? Settings->RegenDelaySeconds : 0.f;
	if (World->GetTimeSeconds() - LastSpendTimeSeconds < RegenDelay)
	{
		return;
	}

	// 비전투일 때는 더 빠른 회복 속도를 쓴다(탐험 편의).
	const float BaseRegen = bInCombat ? Settings->RegenPerSecond : Settings->OutOfCombatRegenPerSecond;
	// 회복 base는 설정(URetrieveStaminaSettings) 소유, 세트/버프 GE가 올린 배율(StaminaRegenMultiplier, 기본 1.0)만 곱한다.
	const float RegenRate = BaseRegen * (AttributeSet ? AttributeSet->GetStaminaRegenMultiplier() : 1.0f);

	const float Cur = GetStamina();
	const float Max = GetMaxStamina();
	if (Cur >= Max || RegenRate <= 0.f)
	{
		return;
	}

	const float Next = FMath::Min(Cur + RegenRate * StaminaRegenTickInterval, Max);
	AbilitySystemComponent->SetNumericAttributeBase(UCombatAttributeSet::GetStaminaAttribute(), Next);
}

void UStaminaComponent::HandleMaxStaminaAttributeChanged(const FOnAttributeChangeData& Data)
{
	OnStaminaChanged.Broadcast(GetStamina(), Data.NewValue);
}

void UStaminaComponent::BroadcastStaminaChanged()
{
	OnStaminaChanged.Broadcast(GetStamina(), GetMaxStamina());
}

void UStaminaComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                      FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UAbilitySystemComponent* ASC = ResolveASC();
	if (!ASC || !AttributeSet)
	{
		return;
	}

	// UI(WBP_Stamina) 연동 완료로 PIE 온스크린 디버그 표시 비활성화.
	// 필요 시 아래 블록과 헤더의 bShowDebugOnScreen 주석을 해제하면 됨.
	//const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	//if (bShowDebugOnScreen && GEngine && OwnerPawn && OwnerPawn->IsLocallyControlled())
	//{
	//	GEngine->AddOnScreenDebugMessage(
	//		/*Key=*/8801, /*TimeToDisplay=*/0.f, FColor::Yellow,
	//		FString::Printf(TEXT("Stamina: %.0f / %.0f"), GetStamina(), GetMaxStamina()));
	//}
}

float UStaminaComponent::GetStamina() const
{
	return AttributeSet ? AttributeSet->GetStamina() : 0.f;
}

float UStaminaComponent::GetMaxStamina() const
{
	return AttributeSet ? AttributeSet->GetMaxStamina() : 0.f;
}

bool UStaminaComponent::HasStamina(float Cost) const
{
	return Cost <= 0.f || GetStamina() >= Cost;
}

void UStaminaComponent::ResetStamina()
{
	if (!AbilitySystemComponent || !AttributeSet)
	{
		return;
	}
	AbilitySystemComponent->SetNumericAttributeBase(UCombatAttributeSet::GetStaminaAttribute(), GetMaxStamina());
}
