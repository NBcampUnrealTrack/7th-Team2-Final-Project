#include "Components/Combat/RetrieveHealthComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "AbilitySystem/Attributes/CombatAttributeSet.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "GameplayEffectExtension.h"
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

void URetrieveHealthComponent::ResetHealth()
{
	if (!AbilitySystemComponent || !AttributeSet)
		return;

	// bDeathStarted 먼저 초기화 → HP 변경 시 재사망 트리거 방지
	bDeathStarted = false;

	// Health를 MaxHealth로 직접 설정 (GE 파이프라인 우회)
	const float MaxHP = AttributeSet->GetMaxHealth();
	AbilitySystemComponent->SetNumericAttributeBase(
		UCombatAttributeSet::GetHealthAttribute(), MaxHP);
	
	UE_LOG(LogTemp, Display, TEXT("[%s] HealthRevocer!"), *GetOwner()->GetName());
}

void URetrieveHealthComponent::Revive()
{
	if (!AbilitySystemComponent || !AttributeSet)
	{
		return;
	}

	FGameplayTagContainer DieTags;
	DieTags.AddTag(RetrieveGameplayTags::Ability_Common_Die);
	AbilitySystemComponent->CancelAbilities(&DieTags);

	FGameplayTagContainer StatusTags;
	StatusTags.AddTag(RetrieveGameplayTags::State_Status_Burn);
	AbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(StatusTags);

	ResetHealth();

	// 부활 보호 시작: 시체 위치(해저드 안/투사체 사이)에서 체력이 채워지는 순간 잔류 위협이
	// 같은 프레임에 재사망시키는 것을 막는다. 리스폰 텔레포트 동안은 충돌이 꺼져 있어
	// 대부분 안전하지만, 해저드 주기 타이머는 오버랩 목록 기반이라 이 창이 필요하다.
	if (const UWorld* World = GetWorld())
	{
		constexpr double ReviveProtectionSeconds = 3.0;
		ReviveProtectionEndRealTime = World->GetRealTimeSeconds() + ReviveProtectionSeconds;
	}
}

bool URetrieveHealthComponent::IsReviveProtectionActive() const
{
	if (ReviveProtectionEndRealTime < 0.0)
	{
		return false;
	}
	const UWorld* World = GetWorld();
	return World && World->GetRealTimeSeconds() <= ReviveProtectionEndRealTime;
}

void URetrieveHealthComponent::KillOwner()
{
	if (!AbilitySystemComponent || !AttributeSet)
	{
		return;
	}

	if (bDeathStarted)
	{
		return;
	}
	
	AbilitySystemComponent->SetNumericAttributeBase(
		UCombatAttributeSet::GetHealthAttribute(), 0.f);
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

	if (!GetOwner()->HasAuthority() || bDeathStarted) return;
	
	if (!(ChangeData.OldValue > 0.f && ChangeData.NewValue <= 0.f)) return;
	
	if (ChangeData.GEModData)
	{
		const FGameplayEffectContextHandle& Ctx = ChangeData.GEModData->EffectSpec.GetContext();
		LastDamageInstigator = Ctx.GetInstigator();
		LastDamageCauser     = Ctx.GetEffectCauser();
	}
	
	bDeathStarted = true;
	
	if (AbilitySystemComponent)
	{
		const FGameplayTag DieTag = RetrieveGameplayTags::Ability_Common_Die;
		FGameplayTagContainer ImmuneTags;
		ImmuneTags.AddTag(DieTag);
		
		AbilitySystemComponent->CancelAbilities(nullptr, &ImmuneTags, nullptr);

		// 사망 시 지속 데미지(도트) 상태이상 제거 → 시체에 GE_Burn이 남아 계속 틱(데미지 플로터)하지 않도록.
		FGameplayTagContainer DotStatusTags;
		DotStatusTags.AddTag(RetrieveGameplayTags::State_Status_Burn);
		AbilitySystemComponent->RemoveActiveEffectsWithGrantedTags(DotStatusTags);

		FGameplayTagContainer ActivationTags;
		ActivationTags.AddTag(DieTag);
		AbilitySystemComponent->TryActivateAbilitiesByTag(ActivationTags);
	}
	
	OnDeathStarted.Broadcast(GetOwner());   
	GetOwner()->ForceNetUpdate();
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
