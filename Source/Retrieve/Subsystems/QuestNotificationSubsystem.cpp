#include "QuestNotificationSubsystem.h"

#include "Core/RetrieveGameState.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "UI/Quest/RetrieveQuestStatus.h"

bool UQuestNotificationSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UQuestNotificationSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// 단계 리스너를 등록하기 전에 시딩, 최초로 관측되는 변화가 항상 실제 diff가 되도록 함
	SeedBaseline();

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(&InWorld);
	StepHandle = MessageSubsystem.RegisterListener<FRetrieveQuestStepPayload>(
		RetrieveGameplayTags::Channel_Quest_StepChanged, this, &UQuestNotificationSubsystem::HandleStepChanged);
	RawHandle = MessageSubsystem.RegisterListener<FRetrieveQuestNotificationPayload>(
		RetrieveGameplayTags::Channel_UI_QuestNotification, this, &UQuestNotificationSubsystem::HandleRawNotification);
}

void UQuestNotificationSubsystem::Deinitialize()
{
	StepHandle.Unregister();
	RawHandle.Unregister();
	Queue.Reset();
	Super::Deinitialize();
}

void UQuestNotificationSubsystem::HandleStepChanged(FGameplayTag Channel, const FRetrieveQuestStepPayload& Message)
{
	RecomputeAndNotify();
}

void UQuestNotificationSubsystem::HandleRawNotification(FGameplayTag Channel,
                                                        const FRetrieveQuestNotificationPayload& Message)
{
	FQuestNotificationEntry Entry;
	Entry.QuestName = Message.QuestName;
	Entry.Kind = Message.Kind;
	Entry.Duration = Message.Duration > 0.f ? Message.Duration : 4.f;
	Enqueue(MoveTemp(Entry));
}

EQuestPhaseInternal UQuestNotificationSubsystem::ComputePhase(const FQuestDefinition& Quest,
                                                              const UQuestBranchComponent& Branch) const
{
	if (!QuestStatus::IsQuestUnlocked(Quest, Branch))
	{
		return EQuestPhaseInternal::Locked;
	}
	if (QuestStatus::AreAllObjectivesComplete(Quest, Branch)) // 목표가 0개인 퀘스트는 false (Active 유지)
	{
		return EQuestPhaseInternal::Completed;
	}
	return EQuestPhaseInternal::Active;
}

void UQuestNotificationSubsystem::SeedBaseline()
{
	Snapshot.Reset();

	UQuestBranchComponent* QuestBranchComponent = GetBranch();
	const UDataTable* Table = GetQuestTable();
	if (QuestBranchComponent && Table)
	{
		static const FString ContextString(TEXT("QuestNotif_Seed"));
		TArray<FQuestDefinition*> Rows;
		Table->GetAllRows<FQuestDefinition>(ContextString, Rows);
		for (const FQuestDefinition* Row : Rows)
		{
			if (Row && Row->QuestId.IsValid())
			{
				Snapshot.Add(Row->QuestId, ComputePhase(*Row, *QuestBranchComponent));
			}
		}
	}
	bBaselineSeeded = true;
}

void UQuestNotificationSubsystem::RecomputeAndNotify()
{
	UQuestBranchComponent* QuestBranchComponent = GetBranch();
	const UDataTable* Table = GetQuestTable();
	if (!QuestBranchComponent || !Table)
	{
		return;
	}

	// 방어적 처리: 어떤 이유로든 단계 변경이 OnWorldBeginPlay보다 먼저 일어나면 시딩하고 이번 프레임은 건너뜀
	if (!bBaselineSeeded)
	{
		SeedBaseline();
		return;
	}

	static const FString ContextString(TEXT("QuestNotif_Recompute"));
	TArray<FQuestDefinition*> Rows;
	Table->GetAllRows<FQuestDefinition>(ContextString, Rows);

	// 한 번의 단계 변경이 같은 프레임에서 여러 퀘스트를 뒤집을 경우(예: 자동 완료) 결정적인 순서를 보장
	Rows.Sort([](const FQuestDefinition& A, const FQuestDefinition& B) { return A.DisplayOrder < B.DisplayOrder; });

	for (const FQuestDefinition* Row : Rows)
	{
		if (!Row || !Row->QuestId.IsValid())
		{
			continue;
		}

		const EQuestPhaseInternal NewPhase = ComputePhase(*Row, *QuestBranchComponent);
		const EQuestPhaseInternal OldPhase = Snapshot.FindRef(Row->QuestId); // 없으면 -> Locked(0)
		if (NewPhase == OldPhase)
		{
			continue;
		}

		if (OldPhase == EQuestPhaseInternal::Locked && NewPhase == EQuestPhaseInternal::Active)
		{
			EnqueueNotification(Row->QuestId, Row->DisplayName, EQuestNotificationKind::Started);
		}
		else if (NewPhase == EQuestPhaseInternal::Completed && OldPhase != EQuestPhaseInternal::Completed)
		{
			// Active->Completed, 또는 드물게 같은 프레임에서 잠금 해제와 완료가 함께 일어나는 경우: Completed만 표시(연달아 뜨지 않게)
			EnqueueNotification(Row->QuestId, Row->DisplayName, EQuestNotificationKind::Completed);
		}

		// 전진이 아닌 변화(원래는 일어나지 않아야 함, 단계는 오직 누적됨)는 스냅샷만 조용히 갱신
		Snapshot.Add(Row->QuestId, NewPhase);
	}
}

void UQuestNotificationSubsystem::EnqueueNotification(FGameplayTag QuestId, const FText& QuestName,
                                                      EQuestNotificationKind Kind)
{
	FQuestNotificationEntry Entry;
	Entry.QuestId = QuestId;
	Entry.QuestName = QuestName;
	Entry.Kind = Kind;
	Entry.Duration = (Kind == EQuestNotificationKind::Completed) ? CompletedDuration : StartedDuration;
	Enqueue(MoveTemp(Entry));
}

void UQuestNotificationSubsystem::Enqueue(FQuestNotificationEntry&& Entry)
{
	Queue.Add(MoveTemp(Entry));

	while (Queue.Num() > FMath::Max(1, MaxQueued))
	{
		Queue.RemoveAt(0); // 오버플로 시 가장 오래된 항목 제거
	}

	OnQueuedDelegate.Broadcast();
}


bool UQuestNotificationSubsystem::DequeueNext(FQuestNotificationEntry& OutEntry)
{
	if (Queue.Num() == 0)
	{
		return false;
	}
	OutEntry = Queue[0];
	Queue.RemoveAt(0);
	return true;
}

void UQuestNotificationSubsystem::RequeueFront(const FQuestNotificationEntry& Entry)
{
	Queue.Insert(Entry, 0); // 중단된 토스트를 보존(컷씬 / 로딩화면 이후 reveal / 위젯 파괴)
}

UQuestBranchComponent* UQuestNotificationSubsystem::GetBranch() const
{
	const UWorld* World = GetWorld();
	ARetrieveGameState* GS = World ? World->GetGameState<ARetrieveGameState>() : nullptr;
	return GS ? GS->GetQuestBranchComponent() : nullptr;
}

const UDataTable* UQuestNotificationSubsystem::GetQuestTable() const
{
	const UWorld* World = GetWorld();
	const ARetrieveGameState* GS = World ? World->GetGameState<ARetrieveGameState>() : nullptr;
	return GS ? GS->GetQuestTable() : nullptr;
}
