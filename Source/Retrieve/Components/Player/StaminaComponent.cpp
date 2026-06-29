#include "Components/Player/StaminaComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Attributes/CombatAttributeSet.h"
#include "GameplayEffect.h"
#include "Engine/Engine.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

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

		if (RegenEffectHandle.IsValid())
		{
			AbilitySystemComponent->RemoveActiveGameplayEffect(RegenEffectHandle);
			RegenEffectHandle.Invalidate();
		}
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

	if (const AActor* Owner = GetOwner(); Owner && Owner->HasAuthority()
		&& StaminaRegenEffect && !RegenEffectHandle.IsValid())
	{
		FGameplayEffectContextHandle Ctx = AbilitySystemComponent->MakeEffectContext();
		Ctx.AddSourceObject(this);
		const FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(StaminaRegenEffect, 1.f, Ctx);
		if (Spec.IsValid())
		{
			RegenEffectHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
		}
	}

	BroadcastStaminaChanged();
	return true;
}

void UStaminaComponent::HandleStaminaAttributeChanged(const FOnAttributeChangeData& Data)
{
	OnStaminaChanged.Broadcast(Data.NewValue, GetMaxStamina());
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
