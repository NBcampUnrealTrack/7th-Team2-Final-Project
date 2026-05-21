#include "Components/PatternCounterComponent.h"

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "AbilitySystem/RetrieveAbilitySystemComponent.h"
#include "Components/RetrievePawnExtensionComponent.h"
#include "Engine/DataTable.h"
#include "GameplayTags/RetrieveGameplayTags.h"

UPatternCounterComponent::UPatternCounterComponent(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UPatternCounterComponent::BeginPlay()
{
	Super::BeginPlay();
	// TODO (AE2-c): GA 몽타주 AnimNotify 연결 후 ASC GameplayEvent 리스너 등록
	// URetrieveAbilitySystemComponent* ASC = GetASC();
	// if (ASC)
	// {
	//     ASC->GenericGameplayEventCallbacks
	//         .FindOrAdd(RetrieveGameplayTags::GameplayEvent_PatternCounterWindow)
	//         .AddUObject(this, &UPatternCounterComponent::HandleCounterWindowEvent);
	// }
}

void UPatternCounterComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	CloseCounterWindow();
	Super::EndPlay(EndPlayReason);
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
	// TODO (B7-b): 플레이어 패링·회피카운터 GA 연결 후 구현
	// 아래는 조건 매칭 뼈대 — B7 작업 시 채워넣는다.
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
		ASC->HandleGameplayEvent(ActivePatternData.CounterEventTag, &EventData);
	}

	if (ActivePatternData.bCanTriggerGroggy)
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now >= GroggyCooldownExpiry)
		{
			FGameplayEventData GroggyEvent;
			GroggyEvent.EventTag   = RetrieveGameplayTags::GameplayEvent_GroggyTrigger;
			GroggyEvent.Instigator = Instigator;
			ASC->HandleGameplayEvent(RetrieveGameplayTags::GameplayEvent_GroggyTrigger, &GroggyEvent);

			// TODO (B6-a): DT_MonsterData.GroggyCooldown을 읽어서 쿨다운 설정
			GroggyCooldownExpiry = Now + 15.f;
		}
	}
}

URetrieveAbilitySystemComponent* UPatternCounterComponent::GetASC() const
{
	URetrievePawnExtensionComponent* PawnExt =
		URetrievePawnExtensionComponent::FindPawnExtensionComponent(GetOwner());
	return PawnExt ? PawnExt->GetRetrieveAbilitySystemComponent() : nullptr;
}
