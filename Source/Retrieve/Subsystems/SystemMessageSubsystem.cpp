#include "SystemMessageSubsystem.h"

#include "Core/RetrieveGameState.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameplayTags/RetrieveGameplayTags.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "Quest/QuestBranchComponent.h"
#include "TimerManager.h"

bool USystemMessageSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void USystemMessageSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	UGameplayMessageSubsystem& MessageSubsystem = UGameplayMessageSubsystem::Get(&InWorld);

	// 원시 경로 등록: 치트 등이 채널에 쏜 {Text, Duration}을 여기서 받아 큐잉한다.
	SystemMessageHandle = MessageSubsystem.RegisterListener<FRetrieveSystemMessagePayload>(
		RetrieveGameplayTags::Channel_UI_SystemMessage, this, &USystemMessageSubsystem::HandleRawMessage);

	// Push 경로 등록: 스텝 완료 시 OnStepTag가 일치하는 메시지 배치를 (딜레이 후) 큐잉한다. 각 클라이언트 로컬.
	StepChangedHandle = MessageSubsystem.RegisterListener<FRetrieveQuestStepPayload>(
		RetrieveGameplayTags::Channel_Quest_StepChanged, this, &USystemMessageSubsystem::HandleStepChanged);
}

void USystemMessageSubsystem::Deinitialize()
{
	SystemMessageHandle.Unregister();
	StepChangedHandle.Unregister();
	if (UWorld* World = GetWorld())
	{
		for (FTimerHandle& Handle : StepMessageTimers)
		{
			World->GetTimerManager().ClearTimer(Handle);
		}
	}
	StepMessageTimers.Reset();
	Queue.Reset();
	Super::Deinitialize();
}

// ---- Public request API (게이트 경로) -------------------------------------

void USystemMessageSubsystem::RequestMessageById(FName RowName)
{
	const UDataTable* Table = GetSystemMessageTable();
	if (!Table || RowName.IsNone())
	{
		return;
	}
	static const FString ContextString(TEXT("SystemMessageById"));
	if (const FSystemMessageRow* Row = Table->FindRow<FSystemMessageRow>(RowName, ContextString, true))
	{
		if (IsRowEligible(RowName, *Row) && !IsRowAlreadyQueued(RowName))
		{
			EnqueueRow(RowName, *Row);
		}
	}
}

void USystemMessageSubsystem::RequestMessageByKey(FGameplayTag KeyTag)
{
	const UDataTable* Table = GetSystemMessageTable();
	if (!Table || !KeyTag.IsValid())
	{
		return;
	}
	static const FString ContextString(TEXT("SystemMessageByKey"));
	const TArray<FName> RowNames = Table->GetRowNames();

	// KeyTag가 같은 자격 행을 전부 훑어 Priority가 가장 낮은(=최우선) 행을 고름
	FName BestRow = NAME_None;
	int32 BestPriority = MAX_int32;
	for (const FName& RowName : RowNames)
	{
		const FSystemMessageRow* Row = Table->FindRow<FSystemMessageRow>(RowName, ContextString, false);
		if (!Row || Row->KeyTag != KeyTag || !IsRowEligible(RowName, *Row))
		{
			continue;
		}
		if (BestRow.IsNone() || Row->Priority < BestPriority) // 동점이면 앞 행 유지
		{
			BestPriority = Row->Priority;
			BestRow = RowName;
		}
	}

	// 승자가 이미 큐에 있으면 그대로 종료
	if (!BestRow.IsNone() && !IsRowAlreadyQueued(BestRow))
	{
		if (const FSystemMessageRow* Row = Table->FindRow<FSystemMessageRow>(BestRow, ContextString, false))
		{
			EnqueueRow(BestRow, *Row);
		}
	}
}

void USystemMessageSubsystem::RequestMessagesByKey(FGameplayTag KeyTag)
{
	const UDataTable* Table = GetSystemMessageTable();
	if (!Table || !KeyTag.IsValid())
	{
		return;
	}
	static const FString ContextString(TEXT("SystemMessagesByKey"));

	// KeyTag가 같은 자격 행을 전부 모은다
	TArray<TPair<FName, const FSystemMessageRow*>> Matches;
	for (const FName& RowName : Table->GetRowNames())
	{
		const FSystemMessageRow* Row = Table->FindRow<FSystemMessageRow>(RowName, ContextString, false);
		if (Row && Row->KeyTag == KeyTag && IsRowEligible(RowName, *Row) && !IsRowAlreadyQueued(RowName))
		{
			Matches.Emplace(RowName, Row);
		}
	}
	// Priority 오름차순, 숫자가 작은 행이 먼저 표시 (1=최우선)
	Matches.Sort([](const TPair<FName, const FSystemMessageRow*>& A, const TPair<FName, const FSystemMessageRow*>& B)
	{
		return A.Value->Priority < B.Value->Priority;
	});
	for (const TPair<FName, const FSystemMessageRow*>& M : Matches)
	{
		EnqueueRow(M.Key, *M.Value);
	}
}

// ---- Raw ingress ----------------------------------------------------------

void USystemMessageSubsystem::HandleRawMessage(FGameplayTag Channel, const FRetrieveSystemMessagePayload& Message)
{
	EnqueueRaw(Message.Text, Message.Duration);
}

// ---- Step-triggered push path ---------------------------------------------

void USystemMessageSubsystem::HandleStepChanged(FGameplayTag Channel, const FRetrieveQuestStepPayload& Message)
{
	// 각 클라이언트가 로컬로 수신
	const UDataTable* Table = GetSystemMessageTable();
	if (!Table || !Message.StepTag.IsValid())
	{
		return;
	}

	static const FString ContextString(TEXT("SystemMessageOnStep"));

	// OnStepTag가 이 스텝인 자격 행들을 모아 배치 단위로 예약
	// KeyTag가 있으면 같은 KeyTag = 하나의 배치(딜레이 후 RequestMessagesByKey가 Priority 순으로 한꺼번에 큐잉).
	// KeyTag가 없으면 단일 메시지로 개별 예약
	TMap<FGameplayTag, float> KeyedDelays; // KeyTag -> Delay(처음 만난 행 기준)
	for (const FName& RowName : Table->GetRowNames())
	{
		const FSystemMessageRow* Row = Table->FindRow<FSystemMessageRow>(RowName, ContextString, false);
		if (!Row || Row->OnStepTag != Message.StepTag || !IsRowEligible(RowName, *Row))
		{
			continue;
		}

		const float Delay = FMath::Max(0.f, Row->TriggerDelaySeconds);
		if (Row->KeyTag.IsValid())
		{
			if (!KeyedDelays.Contains(Row->KeyTag))
			{
				KeyedDelays.Add(Row->KeyTag, Delay);
			}
		}
		else
		{
			ScheduleStepMessage(FGameplayTag(), RowName, Delay);
		}
	}

	for (const TPair<FGameplayTag, float>& Keyed : KeyedDelays)
	{
		ScheduleStepMessage(Keyed.Key, NAME_None, Keyed.Value);
	}
}

void USystemMessageSubsystem::ScheduleStepMessage(FGameplayTag KeyTag, FName RowName, float Delay)
{
	if (Delay <= 0.f)
	{
		FireStepMessage(KeyTag, RowName);
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FTimerHandle Handle;
	World->GetTimerManager().SetTimer(
		Handle, FTimerDelegate::CreateWeakLambda(this, [this, KeyTag, RowName]() { FireStepMessage(KeyTag, RowName); }),
		Delay, false);
	StepMessageTimers.Add(Handle);
}

void USystemMessageSubsystem::FireStepMessage(FGameplayTag KeyTag, FName RowName)
{
	if (KeyTag.IsValid())
	{
		RequestMessagesByKey(KeyTag);
	}
	else if (!RowName.IsNone())
	{
		RequestMessageById(RowName);
	}
}

// ---- Eligibility ----------------------------------------------------------

bool USystemMessageSubsystem::IsRowEligible(FName RowName, const FSystemMessageRow& Row) const
{
	// 1) 1회 표시: 이미 보여준 행이면 탈락
	if (Row.bPlayOnce && PlayedOnceRows.Contains(RowName))
	{
		return false;
	}

	// 2) 쿨다운: 마지막 표시로부터 MinRepeatInterval(초) 안이면 탈락, 0이면 건너뜀
	if (Row.MinRepeatInterval > 0.f)
	{
		if (const double* LastAt = LastShownTime.Find(RowName))
		{
			const UWorld* World = GetWorld();
			const double Now = World ? World->GetTimeSeconds() : 0.0;
			if (Now - *LastAt < Row.MinRepeatInterval)
			{
				return false;
			}
		}
	}

	// 3) 필수 스텝: 전부 완료돼 있어야 통과
	for (const FGameplayTag& RequiredStep : Row.RequiredSteps)
	{
		if (!IsStepCompleted(RequiredStep))
		{
			return false;
		}
	}

	// 4) 금지 스텝: 하나라도 완료면 탈락
	for (const FGameplayTag& ForbiddenStep : Row.ForbiddenSteps)
	{
		if (IsStepCompleted(ForbiddenStep))
		{
			return false;
		}
	}
	return true;
}

bool USystemMessageSubsystem::IsRowAlreadyQueued(FName RowName) const
{
	if (RowName.IsNone()) // 원시 항목은 행 이름이 없어 중복 판정 대상이 아님
	{
		return false;
	}
	return Queue.ContainsByPredicate([RowName](const FSystemMessageEntry& Entry) { return Entry.RowName == RowName; });
}

// ---- Enqueue / dequeue ----------------------------------------------------

void USystemMessageSubsystem::EnqueueRow(FName RowName, const FSystemMessageRow& Row)
{
	FSystemMessageEntry Entry;
	Entry.Text = Row.Text;
	Entry.Duration = Row.Duration;
	Entry.RowName = RowName;
	Entry.bPlayOnce = Row.bPlayOnce;
	Entry.bRequiresDismiss = Row.bRequiresDismiss;
	Enqueue(MoveTemp(Entry));
}

void USystemMessageSubsystem::EnqueueRaw(const FText& Text, float Duration)
{
	FSystemMessageEntry Entry;
	Entry.Text = Text;
	Entry.Duration = Duration > 0.f ? Duration : 4.f; // 0 이하면 기본 4초
	// 행 이름 없음, bPlayOnce=false: 원시 경로는 자격 검사도 1회 표시도 없음.
	Enqueue(MoveTemp(Entry));
}

void USystemMessageSubsystem::Enqueue(FSystemMessageEntry&& Entry)
{
	Queue.Add(MoveTemp(Entry));

	while (Queue.Num() > FMath::Max(1, MaxQueued))
	{
		Queue.RemoveAt(0);
	}

	OnQueuedDelegate.Broadcast();
}

bool USystemMessageSubsystem::DequeueNext(FSystemMessageEntry& OutEntry)
{
	if (Queue.Num() == 0)
	{
		return false;
	}
	OutEntry = Queue[0];
	Queue.RemoveAt(0);

	// 소진 기록은 실제 표시되는 순간에만 함
	if (!OutEntry.RowName.IsNone())
	{
		if (OutEntry.bPlayOnce)
		{
			PlayedOnceRows.Add(OutEntry.RowName);
		}
		if (const UWorld* World = GetWorld())
		{
			LastShownTime.Add(OutEntry.RowName, World->GetTimeSeconds());
		}
	}
	return true;
}

void USystemMessageSubsystem::RequeueFront(const FSystemMessageEntry& Entry)
{
	// 시네마틱 진입 또는 위젯 파괴로 중단된 항목을 보존, 시네마틱이 끝나거나 새 위젯이 뜨면 이 항목부터 다시 표시됨
	Queue.Insert(Entry, 0);
}

// ---- GameState helpers ----------------------------------------------

const UDataTable* USystemMessageSubsystem::GetSystemMessageTable() const
{
	const UWorld* World = GetWorld();
	const ARetrieveGameState* GS = World ? World->GetGameState<ARetrieveGameState>() : nullptr;
	return GS ? GS->GetSystemMessageTable() : nullptr;
}

bool USystemMessageSubsystem::IsStepCompleted(FGameplayTag StepTag) const
{
	const UWorld* World = GetWorld();
	const ARetrieveGameState* GS = World ? World->GetGameState<ARetrieveGameState>() : nullptr;
	const UQuestBranchComponent* Quest = GS ? GS->GetQuestBranchComponent() : nullptr;
	return Quest ? Quest->IsStepCompleted(StepTag) : false;
}
