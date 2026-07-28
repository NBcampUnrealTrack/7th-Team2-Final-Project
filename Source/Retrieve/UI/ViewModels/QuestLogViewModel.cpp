#include "QuestLogViewModel.h"

#include "QuestEntryViewModel.h"
#include "QuestObjectiveEntryViewModel.h"
#include "QuestTrackerViewModel.h"
#include "Components/SlateWrapperTypes.h"
#include "Core/RetrieveGameState.h"
#include "Data/RetrieveDataTableTypes.h"
#include "Quest/QuestBranchComponent.h"
#include "UI/Quest/RetrieveQuestStatus.h"

TArray<UQuestEntryViewModel*> UQuestLogViewModel::GetActiveQuests() const
{
	TArray<UQuestEntryViewModel*> Out;
	Out.Reserve(ActiveQuests.Num());
	for (const TObjectPtr<UQuestEntryViewModel>& Entry : ActiveQuests)
	{
		Out.Add(Entry);
	}
	return Out;
}

TArray<UQuestEntryViewModel*> UQuestLogViewModel::GetCompletedQuests() const
{
	TArray<UQuestEntryViewModel*> Out;
	Out.Reserve(CompletedQuests.Num());
	for (const TObjectPtr<UQuestEntryViewModel>& Entry : CompletedQuests)
	{
		Out.Add(Entry);
	}
	return Out;
}

TArray<UQuestObjectiveEntryViewModel*> UQuestLogViewModel::GetSelectedObjectives() const
{
	TArray<UQuestObjectiveEntryViewModel*> Out;
	Out.Reserve(SelectedObjectives.Num());
	for (const TObjectPtr<UQuestObjectiveEntryViewModel>& Obj : SelectedObjectives)
	{
		Out.Add(Obj);
	}
	return Out;
}

ESlateVisibility UQuestLogViewModel::GetCompletedSectionVisibility() const
{
	return CompletedQuests.Num() > 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
}

void UQuestLogViewModel::InitializeFromGameState(ARetrieveGameState* GameState, UQuestTrackerViewModel* InTracker)
{
	if (!GameState)
	{
		return;
	}
	UWorld* World = GameState->GetWorld();
	if (!World)
	{
		return;
	}
	WorldPtr = World;
	TrackerVM = InTracker;

	if (!StepListenerHandle.IsValid())
	{
		UGameplayMessageSubsystem& Messaging = UGameplayMessageSubsystem::Get(World);
		StepListenerHandle = Messaging.RegisterListener<FRetrieveQuestStepPayload>(
			RetrieveGameplayTags::Channel_Quest_StepChanged,
			[WeakThis = TWeakObjectPtr<UQuestLogViewModel>(this)]
		(FGameplayTag Channel, const FRetrieveQuestStepPayload& Message)
			{
				if (UQuestLogViewModel* VM = WeakThis.Get())
				{
					VM->HandleStepChanged(Channel, Message);
				}
			});
	}

	Recompute(); // 초기 스냅샷, 현재 CompletedSteps에서 시드 (클라이언트 중간 합류에 대응하기 위해)
}

void UQuestLogViewModel::Deinitialize()
{
	if (UWorld* World = WorldPtr.Get())
	{
		if (StepListenerHandle.IsValid())
		{
			UGameplayMessageSubsystem::Get(World).UnregisterListener(StepListenerHandle);
		}
	}
	StepListenerHandle = FGameplayMessageListenerHandle();
}

void UQuestLogViewModel::BeginDestroy()
{
	Deinitialize();
	Super::BeginDestroy();
}

void UQuestLogViewModel::HandleStepChanged(FGameplayTag Channel, const FRetrieveQuestStepPayload& Message)
{
	Recompute(); // 열려 있는 동안 진행되면 라이브로 갱신
}

FGameplayTag UQuestLogViewModel::ResolveTrackedQuestId(const TArray<FQuestDefinition*>& SortedRows,
                                                       const UQuestBranchComponent& Branch) const
{
	// 선택한 퀘스트가 아직 Active면 해당 퀘스트를, 아닌 경우 DisplayOrder 순 첫 Active를 추적 (트래커와 동일한 규칙).
	const FQuestDefinition* DefaultMain = nullptr;
	for (const FQuestDefinition* Row : SortedRows)
	{
		if (!Row || !QuestStatus::IsQuestUnlocked(*Row, Branch) || QuestStatus::AreAllObjectivesComplete(*Row, Branch))
		{
			continue;
		}
		if (TrackedQuestId.IsValid() && Row->QuestId.MatchesTagExact(TrackedQuestId))
		{
			return TrackedQuestId; // 선택한 퀘스트가 아직 유효함
		}
		if (!DefaultMain && Row->Type == EQuestType::Main)
		{
			DefaultMain = Row;
		}
	}
	return DefaultMain ? DefaultMain->QuestId : FGameplayTag();
}

void UQuestLogViewModel::Recompute()
{
	ActiveQuests.Reset();
	CompletedQuests.Reset();

	UWorld* World = WorldPtr.Get();
	ARetrieveGameState* GS = World ? World->GetGameState<ARetrieveGameState>() : nullptr;
	UQuestBranchComponent* Branch = GS ? GS->GetQuestBranchComponent() : nullptr;
	const UDataTable* Table = GS ? GS->GetQuestTable() : nullptr;

	if (Branch && Table)
	{
		static const FString ContextString(TEXT("QuestLogVM_Recompute"));
		TArray<FQuestDefinition*> Rows;
		Table->GetAllRows<FQuestDefinition>(ContextString, Rows);

		Rows.Sort([](const FQuestDefinition& A, const FQuestDefinition& B)
		{
			return A.DisplayOrder < B.DisplayOrder;
		});

		const FGameplayTag ResolvedTracked = ResolveTrackedQuestId(Rows, *Branch);

		for (const FQuestDefinition* Row : Rows)
		{
			if (!Row || !QuestStatus::IsQuestUnlocked(*Row, *Branch))
			{
				continue; // 잠긴 퀘스트는 숨김
			}

			const bool bCompleted = QuestStatus::AreAllObjectivesComplete(*Row, *Branch);
			const bool bTracked = ResolvedTracked.IsValid() && Row->QuestId.MatchesTagExact(ResolvedTracked);

			UQuestEntryViewModel* Entry = NewObject<UQuestEntryViewModel>(this);
			Entry->SetData(Row->QuestId, Row->DisplayName, Row->Type, bCompleted, bTracked);

			if (bCompleted)
			{
				CompletedQuests.Add(Entry);
			}
			else
			{
				ActiveQuests.Add(Entry);
			}
		}

		// 선택 자동 시드: 아직 선택이 없으면 추적 중인 퀘스트를 기본 선택.
		if (!SelectedQuestId.IsValid() && ResolvedTracked.IsValid())
		{
			SelectedQuestId = ResolvedTracked;
		}

		RebuildRightPane(*Branch, *Table);
	}
	else
	{
		SelectedObjectives.Reset();
		bHasSelection = false;
		bCanTrackSelected = false;
	}

	BroadcastListFields();
	OnListsChanged.Broadcast(); // 위젯이 SetListItems로 ListView 갱신
}

void UQuestLogViewModel::RebuildRightPane(const UQuestBranchComponent& Branch, const UDataTable& Table)
{
	SelectedObjectives.Reset();

	const FQuestDefinition* Quest = SelectedQuestId.IsValid() ? FindQuestRow(Table, SelectedQuestId) : nullptr;

	// 선택이 잠겨 사라졌으면(있을 수 없는 경우 방어) 선택 해제
	if (Quest && !QuestStatus::IsQuestUnlocked(*Quest, Branch))
	{
		Quest = nullptr;
	}

	if (!Quest)
	{
		SelectedDisplayName = FText::GetEmpty();
		SelectedDescription = FText::GetEmpty();
		SelectedQuestType = EQuestType::Main;
		bHasSelection = false;
		bCanTrackSelected = false;
		return;
	}

	SelectedDisplayName = Quest->DisplayName;
	SelectedDescription = Quest->Description;
	SelectedQuestType = Quest->Type;
	bHasSelection = true;

	for (const FQuestObjective& Obj : Quest->Objectives)
	{
		UQuestObjectiveEntryViewModel* ObjVM = NewObject<UQuestObjectiveEntryViewModel>(this);
		ObjVM->SetData(Obj.ObjectiveText, Branch.IsStepCompleted(Obj.CompletionTag));
		SelectedObjectives.Add(ObjVM);
	}

	// Active(미완료) 퀘스트고, 아직 추적 중이 아니면 Track 가능
	const bool bSelCompleted = QuestStatus::AreAllObjectivesComplete(*Quest, Branch);
	bCanTrackSelected = !bSelCompleted && !(TrackedQuestId.IsValid() && SelectedQuestId.
		MatchesTagExact(TrackedQuestId));
}

const FQuestDefinition* UQuestLogViewModel::FindQuestRow(const UDataTable& Table, FGameplayTag QuestId) const
{
	static const FString ContextString(TEXT("QuestLogVM_FindRow"));
	TArray<FQuestDefinition*> Rows;
	Table.GetAllRows<FQuestDefinition>(ContextString, Rows);
	for (const FQuestDefinition* Row : Rows)
	{
		if (Row && Row->QuestId.MatchesTagExact(QuestId))
		{
			return Row;
		}
	}
	return nullptr;
}

void UQuestLogViewModel::SelectQuest(FGameplayTag QuestId)
{
	SelectedQuestId = QuestId;

	UWorld* World = WorldPtr.Get();
	ARetrieveGameState* GS = World ? World->GetGameState<ARetrieveGameState>() : nullptr;
	UQuestBranchComponent* Branch = GS ? GS->GetQuestBranchComponent() : nullptr;
	const UDataTable* Table = GS ? GS->GetQuestTable() : nullptr;
	if (Branch && Table)
	{
		RebuildRightPane(*Branch, *Table);
	}

	BroadcastListFields();
	OnListsChanged.Broadcast(); // 우측 목표 ListView 갱신
}

void UQuestLogViewModel::TrackSelectedQuest()
{
	if (!SelectedQuestId.IsValid())
	{
		return;
	}
	TrackedQuestId = SelectedQuestId; // 로컬 미러

	if (UQuestTrackerViewModel* Tracker = TrackerVM.Get())
	{
		Tracker->SetTrackedQuest(SelectedQuestId);
	}

	Recompute(); // 우측 목표 ◇ 마커 + 좌측 Track 버튼 상태 갱신
}

void UQuestLogViewModel::BroadcastListFields()
{
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetActiveQuests);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetCompletedQuests);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetCompletedSectionVisibility);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetSelectedDisplayName);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetSelectedDescription);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetSelectedQuestType);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetSelectedObjectives);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetHasSelection);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetCanTrackSelected);
}
