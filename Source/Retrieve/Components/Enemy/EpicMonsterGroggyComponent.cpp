#include "Components/Enemy/EpicMonsterGroggyComponent.h"

#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Components/Pawn/RetrievePawnExtensionComponent.h"
#include "GameplayTags/RetrieveGameplayTags.h"

UEpicMonsterGroggyComponent::UEpicMonsterGroggyComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UEpicMonsterGroggyComponent::BeginPlay()
{
	Super::BeginPlay();

	if (URetrievePawnExtensionComponent* PawnExtComp =
		URetrievePawnExtensionComponent::FindPawnExtensionComponent(GetOwner()))
	{
		PawnExtComp->OnAbilitySystemInitialized_RegisterAndCall(
			FSimpleMulticastDelegate::FDelegate::CreateUObject(
				this, &UEpicMonsterGroggyComponent::OnAbilitySystemInitialized));
	}
}

void UEpicMonsterGroggyComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GroggyEndTimerHandle);
	}

	if (URetrieveAbilitySystemComponent* ASC = GetASC())
	{
		if (GEAppliedHandle.IsValid())
		{
			ASC->OnGameplayEffectAppliedDelegateToSelf.Remove(GEAppliedHandle);
			GEAppliedHandle.Reset();
		}
		if (GroggyTagHandle.IsValid())
		{
			ASC->RegisterGameplayTagEvent(
				RetrieveGameplayTags::State_Enemy_Groggy,
				EGameplayTagEventType::NewOrRemoved)
				.Remove(GroggyTagHandle);
			GroggyTagHandle.Reset();
		}
	}
	Super::EndPlay(EndPlayReason);
}

void UEpicMonsterGroggyComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bGroggyActive || !GetWorld())
	{
		return;
	}

	const float Remaining = FMath::Max(0.f, GroggyEndTime - GetWorld()->GetTimeSeconds());
	const float Ratio = ActiveGroggyDuration > 0.f ? FMath::Clamp(Remaining / ActiveGroggyDuration, 0.f, 1.f) : 0.f;
	OnGroggyGaugeUpdated.Broadcast(Ratio, true);
}

void UEpicMonsterGroggyComponent::OnAbilitySystemInitialized()
{
	URetrieveAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		return;
	}

	GEAppliedHandle = ASC->OnGameplayEffectAppliedDelegateToSelf.AddUObject(
		this, &UEpicMonsterGroggyComponent::HandleGameplayEffectApplied);

	GroggyTagHandle = ASC->RegisterGameplayTagEvent(
		RetrieveGameplayTags::State_Enemy_Groggy,
		EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UEpicMonsterGroggyComponent::OnGroggyTagChanged);
}

void UEpicMonsterGroggyComponent::HandleGameplayEffectApplied(
	UAbilitySystemComponent* /*Source*/,
	const FGameplayEffectSpec& Spec,
	FActiveGameplayEffectHandle /*Handle*/)
{
	if (bGroggyActive)
	{
		return;
	}

	if (Spec.DynamicAssetTags.HasTag(RetrieveGameplayTags::Attack_Type_Heavy))
	{
		OnHeavyAttackReceived();
	}
}

void UEpicMonsterGroggyComponent::OnHeavyAttackReceived()
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (Now < GroggyCooldownExpiry)
	{
		return;
	}

	HeavyAttackCount = FMath::Min(HeavyAttackCount + 1, HitsRequired);
	OnGroggyGaugeUpdated.Broadcast(GetChargeRatio(), false);

	if (HeavyAttackCount >= HitsRequired)
	{
		TriggerGroggy();
	}
}

void UEpicMonsterGroggyComponent::TriggerGroggy()
{
	URetrieveAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		return;
	}

	HeavyAttackCount = 0;

	FGameplayEventData GroggyEvent;
	GroggyEvent.EventTag = RetrieveGameplayTags::GameplayEvent_GroggyTrigger;
	GroggyEvent.Instigator = GetOwner();
	GroggyEvent.EventMagnitude = GroggyDuration;
	ASC->HandleGameplayEvent(RetrieveGameplayTags::GameplayEvent_GroggyTrigger, &GroggyEvent);

	ApplyGroggyState(GroggyDuration);
}

void UEpicMonsterGroggyComponent::ApplyGroggyState(float Duration)
{
	URetrieveAbilitySystemComponent* ASC = GetASC();
	UWorld* World = GetWorld();
	if (!ASC || !World)
	{
		return;
	}

	const float SafeDuration = FMath::Max(0.1f, Duration);
	bGroggyActive = true;
	ActiveGroggyDuration = SafeDuration;
	GroggyEndTime = World->GetTimeSeconds() + SafeDuration;
	SetComponentTickEnabled(true);

	if (!ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Enemy_Groggy))
	{
		ASC->AddLooseGameplayTag(RetrieveGameplayTags::State_Enemy_Groggy);
	}

	World->GetTimerManager().ClearTimer(GroggyEndTimerHandle);
	World->GetTimerManager().SetTimer(
		GroggyEndTimerHandle,
		this,
		&UEpicMonsterGroggyComponent::ClearGroggyState,
		SafeDuration,
		false);

	OnGroggyGaugeUpdated.Broadcast(1.f, true);
}

void UEpicMonsterGroggyComponent::ResetRespawnState()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(GroggyEndTimerHandle);
	}

	// State.Enemy.Groggy 루즈 태그는 상위(ResetRespawnState)에서 정리되므로, 여기선 내부 상태만 초기화
	bGroggyActive = false;
	HeavyAttackCount = 0;
	GroggyCooldownExpiry = 0.f;

	// 기존 그로기 Tick 중단
	SetComponentTickEnabled(false);

	OnGroggyGaugeUpdated.Broadcast(0.f, false);
}

void UEpicMonsterGroggyComponent::ClearGroggyState()
{
	URetrieveAbilitySystemComponent* ASC = GetASC();
	if (ASC && ASC->HasMatchingGameplayTag(RetrieveGameplayTags::State_Enemy_Groggy))
	{
		ASC->RemoveLooseGameplayTag(RetrieveGameplayTags::State_Enemy_Groggy);
	}

	if (bGroggyActive)
	{
		bGroggyActive = false;
		GroggyCooldownExpiry = GetWorld() ? GetWorld()->GetTimeSeconds() + GroggyCooldown : 0.f;
		OnGroggyGaugeUpdated.Broadcast(0.f, false);
	}

	SetComponentTickEnabled(false);
}

void UEpicMonsterGroggyComponent::OnGroggyTagChanged(const FGameplayTag /*Tag*/, int32 NewCount)
{
	UWorld* World = GetWorld();
	const bool bTimerActive = World && World->GetTimerManager().IsTimerActive(GroggyEndTimerHandle);
	if (bGroggyActive && NewCount == 0 && !bTimerActive)
	{
		bGroggyActive = false;
		GroggyCooldownExpiry = World ? World->GetTimeSeconds() + GroggyCooldown : 0.f;
		OnGroggyGaugeUpdated.Broadcast(0.f, false);
		SetComponentTickEnabled(false);
	}
}

float UEpicMonsterGroggyComponent::GetChargeRatio() const
{
	if (HitsRequired <= 0)
	{
		return 0.f;
	}
	return FMath::Clamp(static_cast<float>(HeavyAttackCount) / static_cast<float>(HitsRequired), 0.f, 1.f);
}

URetrieveAbilitySystemComponent* UEpicMonsterGroggyComponent::GetASC() const
{
	URetrievePawnExtensionComponent* PawnExt =
		URetrievePawnExtensionComponent::FindPawnExtensionComponent(GetOwner());
	return PawnExt ? PawnExt->GetRetrieveAbilitySystemComponent() : nullptr;
}
