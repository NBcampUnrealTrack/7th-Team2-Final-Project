#include "UI/ViewModels/QuestTrackerViewModel.h"

#include "Core/RetrieveGameState.h"
#include "Quest/QuestBranchComponent.h"
#include "UI/Quest/RetrieveQuestStatus.h"

#define LOCTEXT_NAMESPACE "Retrieve.QuestTracker"

void UQuestTrackerViewModel::InitializeFromGameState(ARetrieveGameState* GameState)
{
	if (!GameState)
	{
		return; // 호스트는 항상 유효, 클라이언트에서 드물게 미복제면 다음 EnsureHUDViewModel 호출이 재시도
	}
	UWorld* World = GameState->GetWorld();
	if (!World)
	{
		return;
	}
	WorldPtr = World;

	UGameplayMessageSubsystem& Messaging = UGameplayMessageSubsystem::Get(World);

	if (!StepListenerHandle.IsValid())
	{
		StepListenerHandle = Messaging.RegisterListener<FRetrieveQuestStepPayload>(
			RetrieveGameplayTags::Channel_Quest_StepChanged,
			[WeakThis = TWeakObjectPtr<UQuestTrackerViewModel>(this)]
		(FGameplayTag Channel, const FRetrieveQuestStepPayload& Message)
			{
				if (UQuestTrackerViewModel* VM = WeakThis.Get())
				{
					VM->HandleStepChanged(Channel, Message);
				}
			});
	}

	if (!CinematicListenerHandle.IsValid())
	{
		CinematicListenerHandle = Messaging.RegisterListener<FRetrieveCinematicStatePayload>(
			RetrieveGameplayTags::Channel_Cinematic_Changed,
			[WeakThis = TWeakObjectPtr<UQuestTrackerViewModel>(this)]
		(FGameplayTag Channel, const FRetrieveCinematicStatePayload& Message)
			{
				if (UQuestTrackerViewModel* VM = WeakThis.Get())
				{
					VM->HandleCinematicChanged(Channel, Message);
				}
			});
	}

	bCinematicActive = GameState->GetCinematicState().IsActive();
	Recompute(); // 초기 스냅샷, 현재 CompletedSteps에서 시드 (클라이언트 중간 합류에 대응하기 위해)
}

void UQuestTrackerViewModel::Deinitialize()
{
	if (UWorld* World = WorldPtr.Get())
	{
		UGameplayMessageSubsystem& Messaging = UGameplayMessageSubsystem::Get(World);
		if (StepListenerHandle.IsValid())
		{
			Messaging.UnregisterListener(StepListenerHandle);
		}
		if (CinematicListenerHandle.IsValid())
		{
			Messaging.UnregisterListener(CinematicListenerHandle);
		}
	}
	StepListenerHandle = FGameplayMessageListenerHandle();
	CinematicListenerHandle = FGameplayMessageListenerHandle();
}

void UQuestTrackerViewModel::BeginDestroy()
{
	Deinitialize();
	Super::BeginDestroy();
}

void UQuestTrackerViewModel::SetTrackedQuest(FGameplayTag QuestId)
{
	TrackedQuestId = QuestId;
	Recompute();
}

void UQuestTrackerViewModel::HandleStepChanged(FGameplayTag /*Channel*/, const FRetrieveQuestStepPayload& /*Message*/)
{
	Recompute();
}

void UQuestTrackerViewModel::HandleCinematicChanged(FGameplayTag /*Channel*/,
                                                    const FRetrieveCinematicStatePayload& Message)
{
	bCinematicActive = Message.bActive;
	Recompute();
}

void UQuestTrackerViewModel::Recompute()
{
	UWorld* World = WorldPtr.Get();
	ARetrieveGameState* GS = World ? World->GetGameState<ARetrieveGameState>() : nullptr;
	UQuestBranchComponent* Branch = GS ? GS->GetQuestBranchComponent() : nullptr;
	const UDataTable* Table = GS ? GS->GetQuestTable() : nullptr;

	const FQuestDefinition* Tracked = nullptr;

	if (Branch && Table)
	{
		static const FString Ctx(TEXT("QuestTrackerVM_Recompute"));
		TArray<FQuestDefinition*> Rows;
		Table->GetAllRows<FQuestDefinition>(Ctx, Rows); // 행 포인터는 이 함수 동안 유효

		Rows.Sort([](const FQuestDefinition& A, const FQuestDefinition& B)
		{
			return A.DisplayOrder < B.DisplayOrder;
		});

		const FQuestDefinition* Explicit = nullptr; // 명시적으로 추적 중이며 아직 Active인 퀘스트
		const FQuestDefinition* DefaultMain = nullptr; // DisplayOrder 순 첫 Active 메인
		const FQuestDefinition* DefaultAny = nullptr; // 폴백: 첫 Active(타입 무관)

		for (const FQuestDefinition* Row : Rows)
		{
			if (!Row || !QuestStatus::IsQuestUnlocked(*Row, *Branch) || QuestStatus::AreAllObjectivesComplete(*Row, *Branch))
			{
				continue; // 잠김(숨김) 또는 완료 -> Active 후보 아님
			}
			if (!DefaultAny)
			{
				DefaultAny = Row;
			}
			if (!DefaultMain && Row->Type == EQuestType::Main)
			{
				DefaultMain = Row;
			}
			if (TrackedQuestId.IsValid() && Row->QuestId.MatchesTagExact(TrackedQuestId))
			{
				Explicit = Row;
			}
		}

		// 명시 선택이 아직 Active면 그것을, 아니면 첫 Active 메인, 그것도 없으면 첫 Active(아무거나).
		Tracked = Explicit ? Explicit : (DefaultMain ? DefaultMain : DefaultAny);
	}

	if (Tracked && Branch)
	{
		QuestName = Tracked->DisplayName;
		QuestType = Tracked->Type;

		Objectives.Reset();
		for (const FQuestObjective& Obj : Tracked->Objectives)
		{
			if (!Branch->IsStepCompleted(Obj.CompletionTag)) // 미완료 목표만
			{
				Objectives.Add(Obj.ObjectiveText);
				if (Objectives.Num() >= MaxTrackedObjectives)
				{
					break;
				}
			}
		}
		bVisible = !bCinematicActive;
	}
	else
	{
		QuestName = FText::GetEmpty();
		QuestType = EQuestType::Main;
		Objectives.Reset();
		bVisible = false;
	}

	BroadcastAllFields();
}

FText UQuestTrackerViewModel::GetObjectivesText() const
{
	return FText::Join(FText::FromString(TEXT("\n")), Objectives);
}

void UQuestTrackerViewModel::BroadcastAllFields()
{
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetQuestName);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetQuestType);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetObjectives);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetObjectivesText);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetIsVisible);
	
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetObjective0);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetObjective1);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetObjective2);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetObjective0Visibility);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetObjective1Visibility);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetObjective2Visibility);
}

#undef LOCTEXT_NAMESPACE
