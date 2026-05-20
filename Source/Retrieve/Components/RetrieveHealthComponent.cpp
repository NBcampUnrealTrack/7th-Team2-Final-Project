#include "Components/RetrieveHealthComponent.h"

#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/CombatAttributeSet.h"
#include "Net/UnrealNetwork.h"

URetrieveHealthComponent::URetrieveHealthComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void URetrieveHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(URetrieveHealthComponent, bDeathStarted);
}

void URetrieveHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UninitializeWithAbilitySystem();
	Super::EndPlay(EndPlayReason);
}

void URetrieveHealthComponent::InitializeWithAbilitySystem(URetrieveAbilitySystemComponent* InASC)
{
	AActor* Owner = GetOwner();
	check(Owner);
	
	if (AbilitySystemComponent)
	{
		UninitializeWithAbilitySystem();
	}

	AbilitySystemComponent = InASC;
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogTemp, Error,
		       TEXT("URetrieveHealthComponent on %s: InitializeWithAbilitySystem가 null ASC로 호출되었습니다. ")
		       TEXT("PawnData->bRequiresAbilitySystem / DefaultAbilitySet를 확인하세요."),
		       *GetNameSafe(GetOwner()));
		return;
	}

	AttributeSet = AbilitySystemComponent->GetSet<UCombatAttributeSet>();
	if (!AttributeSet)
	{
		ensureMsgf(false,
		           TEXT("URetrieveHealthComponent on %s: ASC에 UCombatAttributeSet이 없습니다. ")
		           TEXT("PawnData->DefaultAbilitySet을 확인하세요."),
		           *GetNameSafe(GetOwner()));
		AbilitySystemComponent = nullptr;
		return;
	}

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UCombatAttributeSet::GetHealthAttribute())
	                      .AddUObject(this, &ThisClass::HandleHealthChanged);
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UCombatAttributeSet::GetMaxHealthAttribute())
	                      .AddUObject(this, &ThisClass::HandleMaxHealthChanged);

	OnHealthChanged.Broadcast(AttributeSet->GetHealth());
	OnMaxHealthChanged.Broadcast(AttributeSet->GetMaxHealth());
}

void URetrieveHealthComponent::UninitializeWithAbilitySystem()
{
	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UCombatAttributeSet::GetHealthAttribute()).
		                        RemoveAll(this);
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UCombatAttributeSet::GetMaxHealthAttribute()).
		                        RemoveAll(this);
	}
	AttributeSet = nullptr;
	AbilitySystemComponent = nullptr;
}

float URetrieveHealthComponent::GetHealth() const
{
	return AttributeSet ? AttributeSet->GetHealth() : 0.0f;
}

float URetrieveHealthComponent::GetMaxHealth() const
{
	return AttributeSet ? AttributeSet->GetMaxHealth() : 0.0f;
}

void URetrieveHealthComponent::HandleHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	OnHealthChanged.Broadcast(ChangeData.NewValue);

	if (GetOwner()->HasAuthority() && !bDeathStarted && ChangeData.NewValue <= 0.f)
	{
		bDeathStarted = true;
		OnDeathStarted.Broadcast(GetOwner());
		GetOwner()->ForceNetUpdate();
	}
}

void URetrieveHealthComponent::HandleMaxHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	OnMaxHealthChanged.Broadcast(ChangeData.NewValue);
}

void URetrieveHealthComponent::OnRep_DeathStarted()
{
	if (bDeathStarted)
	{
		OnDeathStarted.Broadcast(GetOwner());
	}
}
