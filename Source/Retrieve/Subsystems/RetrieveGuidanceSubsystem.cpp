#include "Subsystems/RetrieveGuidanceSubsystem.h"

#include "Bark/BarkSubsystem.h"
#include "Core/RetrieveGameState.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/GameplayMessages/RetrieveGameplayMessageTypes.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "Quest/QuestBranchComponent.h"
#include "Subsystems/SystemMessageSubsystem.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "Retrieve.Guidance"

void URetrieveGuidanceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LastObjectiveChangeTime = FPlatformTime::Seconds();

	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &URetrieveGuidanceSubsystem::TickGuidance), 1.0f);
}

void URetrieveGuidanceSubsystem::Deinitialize()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}

	Super::Deinitialize();
}

UWorld* URetrieveGuidanceSubsystem::GetActiveWorld() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetWorld() : nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// 현재 목표
// ─────────────────────────────────────────────────────────────────────────────
void URetrieveGuidanceSubsystem::UpdateTrackedObjective(
	const FText& InQuestName, const FText& InObjectiveText, FGameplayTag InStepTag)
{
	const bool bChanged = !InStepTag.MatchesTagExact(StepTag) || !InObjectiveText.EqualTo(ObjectiveText);

	QuestName = InQuestName;
	ObjectiveText = InObjectiveText;
	StepTag = InStepTag;

	if (bChanged)
	{
		// 목표가 실제로 넘어간 순간에만 정체 타이머를 리셋한다.
		LastObjectiveChangeTime = FPlatformTime::Seconds();
	}
}

FText URetrieveGuidanceSubsystem::GetObjectiveBriefLine() const
{
	if (!HasObjective())
	{
		return FText::GetEmpty();
	}

	return FText::Format(LOCTEXT("ObjectiveBrief", "현재 목표 · {0}"), ObjectiveText);
}

// ─────────────────────────────────────────────────────────────────────────────
// 재확인 / 코치마크 / 브리핑
// ─────────────────────────────────────────────────────────────────────────────
void URetrieveGuidanceSubsystem::RequestObjectiveReminder()
{
	UWorld* World = GetActiveWorld();
	if (!World)
	{
		return;
	}

	FRetrieveObjectiveReminderPayload Payload;
	Payload.QuestName = QuestName;
	Payload.ObjectiveText = ObjectiveText;
	Payload.StepTag = StepTag;

	UGameplayMessageSubsystem::Get(World).BroadcastMessage(
		RetrieveGameplayTags::Channel_UI_ObjectiveReminder, Payload);
}

void URetrieveGuidanceSubsystem::TriggerFirstTimeCoach(FGameplayTag KeyTag)
{
	if (!KeyTag.IsValid() || FiredCoachKeys.Contains(KeyTag))
	{
		return;
	}
	FiredCoachKeys.Add(KeyTag);

	UWorld* World = GetActiveWorld();
	if (!World)
	{
		return;
	}

	if (USystemMessageSubsystem* MessageSub = World->GetSubsystem<USystemMessageSubsystem>())
	{
		// DT_SystemMessage에 같은 KeyTag 행이 없으면 조용히 아무것도 안 뜬다(데이터 미작성 허용).
		MessageSub->RequestMessagesByKey(KeyTag);
	}
}

void URetrieveGuidanceSubsystem::PlayIntroBriefing()
{
	TriggerFirstTimeCoach(RetrieveGameplayTags::UI_Guidance_Intro);
}

void URetrieveGuidanceSubsystem::ResetForNewGame()
{
	FiredCoachKeys.Reset();
	LastObjectiveChangeTime = FPlatformTime::Seconds();
	LastHintTime = -1000.0;
}

// ─────────────────────────────────────────────────────────────────────────────
// 힌트
// ─────────────────────────────────────────────────────────────────────────────
void URetrieveGuidanceSubsystem::EnsureWorldHooks(UWorld* World)
{
	if (!World || HookedWorld.Get() == World)
	{
		return;
	}
	HookedWorld = World;

	UGameplayMessageSubsystem& Messaging = UGameplayMessageSubsystem::Get(World);

	// 최초 경험 코치마크: 볼륨을 배치하지 않아도 "처음 겪는 순간"에 자동으로 뜬다.
	PickupHandle = Messaging.RegisterListener<FRetrievePickupToastPayload>(
		RetrieveGameplayTags::Channel_UI_PickupToast,
		this, &URetrieveGuidanceSubsystem::HandlePickupToast);

	RestedHandle = Messaging.RegisterListener<FRetrievePlayerRestedPayload>(
		RetrieveGameplayTags::Channel_Player_Rested,
		this, &URetrieveGuidanceSubsystem::HandleRested);

	DiedHandle = Messaging.RegisterListener<FPlayerDiedPayload>(
		RetrieveGameplayTags::Channel_Player_Died,
		this, &URetrieveGuidanceSubsystem::HandlePlayerDied);

	// 새 게임(완료한 스텝이 하나도 없음)이면 시작 브리핑을 띄운다.
	if (const ARetrieveGameState* GS = World->GetGameState<ARetrieveGameState>())
	{
		if (const UQuestBranchComponent* Branch = GS->GetQuestBranchComponent())
		{
			TArray<FGameplayTag> Completed;
			FGameplayTag TrackerStep;
			TMap<FName, FGameplayTag> Choices;
			Branch->MakeQuestSaveData(Completed, TrackerStep, Choices);

			if (Completed.Num() == 0)
			{
				PlayIntroBriefing();
			}
		}
	}
}

void URetrieveGuidanceSubsystem::HandlePickupToast(
	FGameplayTag /*Channel*/, const FRetrievePickupToastPayload& /*Message*/)
{
	TriggerFirstTimeCoach(RetrieveGameplayTags::UI_Guidance_FirstPickup);
}

void URetrieveGuidanceSubsystem::HandleRested(
	FGameplayTag /*Channel*/, const FRetrievePlayerRestedPayload& /*Message*/)
{
	TriggerFirstTimeCoach(RetrieveGameplayTags::UI_Guidance_FirstBonfire);
}

void URetrieveGuidanceSubsystem::HandlePlayerDied(
	FGameplayTag /*Channel*/, const FPlayerDiedPayload& /*Message*/)
{
	// 죽고 나면 방향 감각을 가장 많이 잃는다. 리스폰 직후 목표를 한 번 더 알려준다.
	UWorld* World = GetActiveWorld();
	if (!World || RespawnReminderDelay <= 0.0f)
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		RespawnReminderTimer,
		FTimerDelegate::CreateUObject(this, &URetrieveGuidanceSubsystem::RequestObjectiveReminder),
		RespawnReminderDelay, false);
}

bool URetrieveGuidanceSubsystem::TickGuidance(float /*DeltaTime*/)
{
	EnsureWorldHooks(GetActiveWorld());

	if (HintIdleSeconds <= 0.0f || !HasObjective())
	{
		return true;
	}

	const double Now = FPlatformTime::Seconds();
	if (Now - LastObjectiveChangeTime < HintIdleSeconds)
	{
		return true;
	}
	if (Now - LastHintTime < HintRepeatSeconds)
	{
		return true;
	}

	FireHint();
	LastHintTime = Now;
	return true; // 계속 틱
}

void URetrieveGuidanceSubsystem::FireHint()
{
	UWorld* World = GetActiveWorld();
	if (!World)
	{
		return;
	}

	UBarkSubsystem* BarkSub = World->GetSubsystem<UBarkSubsystem>();
	if (!BarkSub)
	{
		return;
	}

	// 스텝 전용 힌트가 있으면 그것을, 없으면 공용 힌트를 쓴다.
	// Quest.Step.FoundVillage        → Bark.Hint.FoundVillage
	// Quest.Step.TalkedToLumen.Village → Bark.Hint.TalkedToLumen.Village
	// (마지막 마디만 떼면 TalkedToLumen.Village가 "Village"가 되어 다른 스텝과 충돌한다)
	if (StepTag.IsValid())
	{
		FString StepName = StepTag.ToString();
		StepName.RemoveFromStart(TEXT("Quest.Step."), ESearchCase::CaseSensitive);

		const FGameplayTag StepHintTag = FGameplayTag::RequestGameplayTag(
			FName(*FString::Printf(TEXT("Bark.Hint.%s"), *StepName)), /*ErrorIfNotFound=*/false);
		if (StepHintTag.IsValid())
		{
			BarkSub->RequestBarkByKey(StepHintTag);
			return;
		}
	}

	BarkSub->RequestBarkByKey(RetrieveGameplayTags::Bark_Hint_Generic);
}

#undef LOCTEXT_NAMESPACE
