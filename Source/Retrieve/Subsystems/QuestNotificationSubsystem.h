#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "QuestNotificationSubsystem.generated.h"

class UQuestBranchComponent;
struct FQuestDefinition;

/** CompletedSteps로부터 도출되는 내부 퀘스트별 단계. Locked=0이므로 TMap::FindRef의 기본값. */
enum class EQuestPhaseInternal : uint8
{
	Locked = 0,
	Active,
	Completed
};

/** 버퍼링된 토스트 하나: 표시 이름 + 종류 + 유지 시간. */
struct FQuestNotificationEntry
{
	FText QuestName;
	EQuestNotificationKind Kind = EQuestNotificationKind::Started;
	float Duration = 4.f;
	FGameplayTag QuestId;
};

/**
 * 대기 중인 퀘스트 토스트 큐를 소유합니다. HUD가 존재하기 전에 발생한 전환도 위젯이 나타날 때까지 살아남습니다.
 * 입력 경로 2, 출력 경로 1:
 *   - Derived: Channel.Quest.StepChanged -> 각 퀘스트의 단계 재계산 -> 스냅샷과 diff ->
 *     전진 전환을 큐에 넣음 (Locked->Active = Started, ->Completed = Completed).
 *   - Raw(원시): Channel.UI.QuestNotification 브로드캐스트(치트) -> 텍스트를 직접 큐에 넣음(diff 없음).
 *   - 출력: 위젯이 DequeueNext()로 비움(클라이언트 로컬).
 */
UCLASS()
class RETRIEVE_API UQuestNotificationSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	bool HasPending() const { return Queue.Num() > 0; }
	bool DequeueNext(FQuestNotificationEntry& OutEntry);
	void RequeueFront(const FQuestNotificationEntry& Entry);
	FSimpleMulticastDelegate& OnQueued() { return OnQueuedDelegate; }

	/** 호스트 새 게임: (방금 리셋된) 퀘스트 상태로부터 베이스라인을 다시 시딩하여
	 * 다음 CompleteStep이 깨끗한 전환으로 읽히도록 합니다. 반복 호출해도 안전합니다. */
	void ResetBaseline() { SeedBaseline(); }

	// ---- UWorldSubsystem ----
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

protected:
	void HandleStepChanged(FGameplayTag Channel, const FRetrieveQuestStepPayload& Message);
	void HandleRawNotification(FGameplayTag Channel, const FRetrieveQuestNotificationPayload& Message);

	void SeedBaseline();
	void RecomputeAndNotify();
	EQuestPhaseInternal ComputePhase(const FQuestDefinition& Quest, const UQuestBranchComponent& Branch) const;
	void EnqueueNotification(FGameplayTag QuestId, const FText& QuestName, EQuestNotificationKind Kind);
	void Enqueue(FQuestNotificationEntry&& Entry);

	UQuestBranchComponent* GetBranch() const;
	const UDataTable* GetQuestTable() const;

private:
	TArray<FQuestNotificationEntry> Queue;

	/** QuestId -> 마지막으로 관측된 단계. 매 단계 변경마다 diff되며, 전진 전환만 토스트를 만듭니다. */
	TMap<FGameplayTag, EQuestPhaseInternal> Snapshot;
	bool bBaselineSeeded = false;

	FGameplayMessageListenerHandle StepHandle;
	FGameplayMessageListenerHandle RawHandle;
	FSimpleMulticastDelegate OnQueuedDelegate;

	int32 MaxQueued = 6;
	float StartedDuration = 4.f;
	float CompletedDuration = 5.f;
};
