#include "Components/Enemy/EnemyPoiseComponent.h"

#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystem/Attributes/CombatAttributeSet.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Components/Pawn/RetrievePawnExtensionComponent.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameplayTags/RetrieveGameplayTags.h"

UEnemyPoiseComponent::UEnemyPoiseComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UEnemyPoiseComponent::BeginPlay()
{
	Super::BeginPlay();

	if (URetrievePawnExtensionComponent* PawnExtComp =
		URetrievePawnExtensionComponent::FindPawnExtensionComponent(GetOwner()))
	{
		PawnExtComp->OnAbilitySystemInitialized_RegisterAndCall(
			FSimpleMulticastDelegate::FDelegate::CreateUObject(
				this, &UEnemyPoiseComponent::OnAbilitySystemInitialized));
	}
}

void UEnemyPoiseComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (URetrieveAbilitySystemComponent* ASC = GetASC())
	{
		if (HitNormalEventHandle.IsValid())
		{
			ASC->GenericGameplayEventCallbacks
				.FindOrAdd(RetrieveGameplayTags::GameplayEvent_Hit_Normal)
				.Remove(HitNormalEventHandle);
		}
		if (HitHeavyEventHandle.IsValid())
		{
			ASC->GenericGameplayEventCallbacks
				.FindOrAdd(RetrieveGameplayTags::GameplayEvent_Hit_Heavy)
				.Remove(HitHeavyEventHandle);
		}
	}

	HitNormalEventHandle.Reset();
	HitHeavyEventHandle.Reset();

	Super::EndPlay(EndPlayReason);
}

void UEnemyPoiseComponent::InitializeFromMonsterData(const FMonsterDataRow& Row, bool bIgnorePendingPoiseDamage)
{
	MaxPoise = FMath::Max(0.f, Row.MaxPoise);
	PoiseDamageMultiplier = FMath::Max(0.f, Row.PoiseDamageMultiplier);
	PoiseGroggyDuration = FMath::Max(0.f, Row.PoiseGroggyDuration);
	GroggyCooldown = FMath::Max(0.f, Row.GroggyCooldown);
	NextGroggyAllowedTime = 0.f;
	bIgnoreNextPoiseDamage = bIgnorePendingPoiseDamage;

	if (URetrieveAbilitySystemComponent* ASC = GetASC())
	{
		ASC->SetNumericAttributeBase(UCombatAttributeSet::GetMaxPoiseAttribute(), MaxPoise);
		ResetPoise();
	}
}

void UEnemyPoiseComponent::OnAbilitySystemInitialized()
{
	URetrieveAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		return;
	}

	if (!HitNormalEventHandle.IsValid())
	{
		HitNormalEventHandle = ASC->GenericGameplayEventCallbacks
			.FindOrAdd(RetrieveGameplayTags::GameplayEvent_Hit_Normal)
			.AddUObject(this, &UEnemyPoiseComponent::HandleHitEvent);
	}
	if (!HitHeavyEventHandle.IsValid())
	{
		HitHeavyEventHandle = ASC->GenericGameplayEventCallbacks
			.FindOrAdd(RetrieveGameplayTags::GameplayEvent_Hit_Heavy)
			.AddUObject(this, &UEnemyPoiseComponent::HandleHitEvent);
	}

	ASC->SetNumericAttributeBase(UCombatAttributeSet::GetMaxPoiseAttribute(), MaxPoise);
	ResetPoise();
}

void UEnemyPoiseComponent::HandleHitEvent(const FGameplayEventData* Payload)
{
	if (!Payload || !IsPoiseEnabled())
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	URetrieveAbilitySystemComponent* ASC = GetASC();
	if (!ASC ||
		ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Enemy_Dead) ||
		ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Enemy_Groggy))
	{
		return;
	}

	ApplyPoiseDamage(Payload->EventMagnitude, const_cast<AActor*>(Payload->Instigator.Get()));
}

void UEnemyPoiseComponent::ApplyPoiseDamage(float DamageDone, AActor* Instigator)
{
	URetrieveAbilitySystemComponent* ASC = GetASC();
	if (!ASC || DamageDone <= 0.f)
	{
		return;
	}

	if (bIgnoreNextPoiseDamage)
	{
		bIgnoreNextPoiseDamage = false;
		ResetPoise();
		return;
	}

	const UWorld* World = GetWorld();
	if (World && World->GetTimeSeconds() < NextGroggyAllowedTime)
	{
		ResetPoise();
		return;
	}

	const float PoiseDamage = DamageDone * PoiseDamageMultiplier;
	if (PoiseDamage <= 0.f)
	{
		return;
	}

	const float CurrentPoise = ASC->GetNumericAttribute(UCombatAttributeSet::GetPoiseAttribute());
	const float NewPoise = FMath::Clamp(CurrentPoise - PoiseDamage, 0.f, MaxPoise);
	ASC->SetNumericAttributeBase(UCombatAttributeSet::GetPoiseAttribute(), NewPoise);

	if (NewPoise <= 0.f)
	{
		TryTriggerGroggy(Instigator);
		ResetPoise();
	}
}

void UEnemyPoiseComponent::ResetPoise() const
{
	if (URetrieveAbilitySystemComponent* ASC = GetASC())
	{
		ASC->SetNumericAttributeBase(UCombatAttributeSet::GetPoiseAttribute(), MaxPoise);
	}
}

void UEnemyPoiseComponent::ResetRespawnState()
{
	NextGroggyAllowedTime = 0.f;
	bIgnoreNextPoiseDamage = false;
	ResetPoise();
}

void UEnemyPoiseComponent::TryTriggerGroggy(AActor* Instigator)
{
	UWorld* World = GetWorld();
	URetrieveAbilitySystemComponent* ASC = GetASC();
	if (!World || !ASC || PoiseGroggyDuration <= 0.f)
	{
		return;
	}

	const float Now = World->GetTimeSeconds();
	if (Now < NextGroggyAllowedTime)
	{
		return;
	}

	FGameplayEventData GroggyEvent;
	GroggyEvent.EventTag = RetrieveGameplayTags::GameplayEvent_GroggyTrigger;
	GroggyEvent.Instigator = Instigator ? Instigator : GetOwner();
	GroggyEvent.Target = GetOwner();
	GroggyEvent.EventMagnitude = PoiseGroggyDuration;
	ASC->HandleGameplayEvent(RetrieveGameplayTags::GameplayEvent_GroggyTrigger, &GroggyEvent);

	NextGroggyAllowedTime = Now + PoiseGroggyDuration + GroggyCooldown;
}

URetrieveAbilitySystemComponent* UEnemyPoiseComponent::GetASC() const
{
	URetrievePawnExtensionComponent* PawnExt =
		URetrievePawnExtensionComponent::FindPawnExtensionComponent(GetOwner());
	return PawnExt ? PawnExt->GetRetrieveAbilitySystemComponent() : nullptr;
}

bool UEnemyPoiseComponent::IsPoiseEnabled() const
{
	return MaxPoise > 0.f;
}
