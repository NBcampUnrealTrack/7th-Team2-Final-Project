#include "Components/Enemy/PatternCounterComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Components/Pawn/RetrievePawnExtensionComponent.h"
#include "Engine/DataTable.h"
#include "GameFramework/Pawn.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Player/RetrievePlayerState.h"

UPatternCounterComponent::UPatternCounterComponent(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPatternCounterComponent::BeginPlay()
{
	Super::BeginPlay();
	
	// ASC 초기화 시 등록되도록 예약 걸기
	if (URetrievePawnExtensionComponent* PawnExtComp = 
		URetrievePawnExtensionComponent::FindPawnExtensionComponent(GetOwner()))
	{
		PawnExtComp->OnAbilitySystemInitialized_RegisterAndCall(
			FSimpleMulticastDelegate::FDelegate::CreateUObject(
				this, &UPatternCounterComponent::OnAbilitySystemInitialized));
	}
}

void UPatternCounterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (CounterWindowEventHandle.IsValid())
	{
		if (URetrieveAbilitySystemComponent* ASC = GetASC())
		{
			ASC->GenericGameplayEventCallbacks
			   .FindOrAdd(RetrieveGameplayTags::GameplayEvent_PatternCounterWindow)
			   .Remove(CounterWindowEventHandle);
		}
		CounterWindowEventHandle.Reset();
	}

	if (HitNormalEventHandle.IsValid())
	{
		if (URetrieveAbilitySystemComponent* ASC = GetASC())
		{
			ASC->GenericGameplayEventCallbacks
			   .FindOrAdd(RetrieveGameplayTags::GameplayEvent_Hit_Normal)
			   .Remove(HitNormalEventHandle);
		}
		HitNormalEventHandle.Reset();
	}

	if (HitHeavyEventHandle.IsValid())
	{
		if (URetrieveAbilitySystemComponent* ASC = GetASC())
		{
			ASC->GenericGameplayEventCallbacks
			   .FindOrAdd(RetrieveGameplayTags::GameplayEvent_Hit_Heavy)
			   .Remove(HitHeavyEventHandle);
		}
		HitHeavyEventHandle.Reset();
	}
	
	CloseCounterWindow();
	Super::EndPlay(EndPlayReason);
}

void UPatternCounterComponent::OnAbilitySystemInitialized()
{
	URetrieveAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		return;
	}
	
	CounterWindowEventHandle = ASC->GenericGameplayEventCallbacks
		.FindOrAdd(RetrieveGameplayTags::GameplayEvent_PatternCounterWindow)
		.AddUObject(this,&UPatternCounterComponent::HandleCounterWindowEvent);

	HitNormalEventHandle = ASC->GenericGameplayEventCallbacks
		.FindOrAdd(RetrieveGameplayTags::GameplayEvent_Hit_Normal)
		.AddUObject(this, &UPatternCounterComponent::HandleHitEvent);

	HitHeavyEventHandle = ASC->GenericGameplayEventCallbacks
		.FindOrAdd(RetrieveGameplayTags::GameplayEvent_Hit_Heavy)
		.AddUObject(this, &UPatternCounterComponent::HandleHitEvent);
}

void UPatternCounterComponent::HandleCounterWindowEvent(const FGameplayEventData* Payload)
{
	const float Duration = Payload ? Payload->EventMagnitude : 0.f;
	OpenCounterWindow(Duration);
}

void UPatternCounterComponent::HandleHitEvent(const FGameplayEventData* Payload)
{
	if (!bWindowOpen || !Payload)
	{
		return;
	}

	AActor* Instigator = const_cast<AActor*>(Payload->Instigator.Get());
	const FGameplayTag ElementTag = ResolveElementTagFromInstigator(Instigator);
	TryCounter(FGameplayTag(), ElementTag, Instigator);
}

FGameplayTag UPatternCounterComponent::ResolveElementTagFromInstigator(AActor* Instigator) const
{
	const APawn* InstigatorPawn = Cast<APawn>(Instigator);
	const ARetrievePlayerState* RetrievePlayerState = InstigatorPawn ? InstigatorPawn->GetPlayerState<ARetrievePlayerState>() : nullptr;
	if (RetrievePlayerState)
	{
		return RetrievePlayerState->GetCurrentElementTag();
	}

	UAbilitySystemComponent* InstigatorASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Instigator);
	if (!InstigatorASC)
	{
		return FGameplayTag();
	}

	if (InstigatorASC->HasMatchingGameplayTag(RetrieveGameplayTags::Element_Wind))
	{
		return RetrieveGameplayTags::Element_Wind;
	}
	if (InstigatorASC->HasMatchingGameplayTag(RetrieveGameplayTags::Element_Fire))
	{
		return RetrieveGameplayTags::Element_Fire;
	}
	if (InstigatorASC->HasMatchingGameplayTag(RetrieveGameplayTags::Element_Water))
	{
		return RetrieveGameplayTags::Element_Water;
	}

	return FGameplayTag();
}

void UPatternCounterComponent::SetActivePatternRow(FName RowName, UDataTable* Table)
{
	ActivePatternRowName = RowName;

	if (Table && !RowName.IsNone())
	{
		const FMonsterPatternRow* Row = Table->FindRow<FMonsterPatternRow>(RowName, TEXT("UPatternCounterComponent"));
		if (Row)
		{
			ActivePatternData = *Row;
		}
	}
}

void UPatternCounterComponent::OpenCounterWindow(float WindowDuration)
{
	if (bWindowOpen)
	{
		CloseCounterWindow();
	}

	bWindowOpen = true;

	const float Duration = WindowDuration > 0.f ? WindowDuration : DefaultWindowDuration;
	GetWorld()->GetTimerManager().SetTimer(
		WindowTimerHandle,
		this,
		&UPatternCounterComponent::OnWindowExpired,
		Duration,
		false);
}

void UPatternCounterComponent::CloseCounterWindow()
{
	bWindowOpen = false;
	GetWorld()->GetTimerManager().ClearTimer(WindowTimerHandle);
}

void UPatternCounterComponent::TryCounter(FGameplayTag ActionTag, FGameplayTag ElementTag, AActor* Instigator)
{
	if (!bWindowOpen)
	{
		return;
	}

	const bool bActionMatch = !ActivePatternData.RequiredActionTag.IsValid()
		|| ActionTag.MatchesTag(ActivePatternData.RequiredActionTag);

	const bool bElementMatch = !ActivePatternData.RequiredElementTag.IsValid()
		|| ElementTag.MatchesTag(ActivePatternData.RequiredElementTag);

	if (bActionMatch && bElementMatch)
	{
		ApplyCounterResult(Instigator);
		CloseCounterWindow();
	}
}

void UPatternCounterComponent::OnWindowExpired()
{
	bWindowOpen = false;
}

void UPatternCounterComponent::ApplyCounterResult(AActor* Instigator)
{
	URetrieveAbilitySystemComponent* ASC = GetASC();
	if (!ASC)
	{
		return;
	}

	if (ActivePatternData.CounterEventTag.IsValid())
	{
		FGameplayEventData EventData;
		EventData.EventTag   = ActivePatternData.CounterEventTag;
		EventData.Instigator = Instigator;
		EventData.EventMagnitude  = ActivePatternData.GroggyDuration;
		ASC->HandleGameplayEvent(ActivePatternData.CounterEventTag, &EventData);
	}

	if (ActivePatternData.bCanTriggerGroggy && ActivePatternData.GroggyDuration > 0.f)
	{
		const float Now = GetWorld()->GetTimeSeconds();
		const float GroggyDur = ActivePatternData.GroggyDuration; 
		
		if (Now >= GroggyCooldownExpiry)
		{
			FGameplayEventData GroggyEvent;
			GroggyEvent.EventTag   = RetrieveGameplayTags::GameplayEvent_GroggyTrigger;
			GroggyEvent.Instigator = Instigator;
			GroggyEvent.EventMagnitude = GroggyDur;
			ASC->HandleGameplayEvent(RetrieveGameplayTags::GameplayEvent_GroggyTrigger, &GroggyEvent);

			// TODO (B6-a): DT_MonsterData.GroggyCooldown을 읽어서 쿨다운 설정
			GroggyCooldownExpiry = Now + GroggyDur + GroggyCooldown;
		}
	}
}

URetrieveAbilitySystemComponent* UPatternCounterComponent::GetASC() const
{
	URetrievePawnExtensionComponent* PawnExt =
		URetrievePawnExtensionComponent::FindPawnExtensionComponent(GetOwner());
	return PawnExt ? PawnExt->GetRetrieveAbilitySystemComponent() : nullptr;
}
