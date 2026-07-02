#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Subsystems/WorldSubsystem.h"
#include "SystemMessageSubsystem.generated.h"

struct FSystemMessageRow;
struct FRetrieveSystemMessagePayload;

/**
 * 큐에 쌓이는 대기 메시지 하나. 표시에 필요한 값(Text, Duration) + 표시 후 이력 기록에 쓸 행 이름(RowName)을 담습니다.
 * 이력 = 1회 표시 여부 + 쿨다운(마지막 표시 시각). 치트/broadcast로 들어온 항목은 RowName이 없으며 bPlayOnce=false입니다.
 */
struct FSystemMessageEntry
{
	FText Text;
	float Duration = 4.f;
	FName RowName = NAME_None;
	bool bPlayOnce = false;
};

/**
 * 시스템 메시지의 대기 큐를 소유하는 월드 서브시스템. 큐, 1회 표시, 쿨다운 상태를 관리합니다.
 * 입구 2, 출구 1:
 *   - 게이트: RequestMessageById/ByKey -> 테이블에 등록된 메시지를 규칙 검사 후 표시
 *   - 원시: Channel.UI.SystemMessage broadcast(치트 등) 수신 -> 텍스트를 그냥 받아서 바로 표시
 *   - 출구: 위젯이 DequeueNext()로 하나씩 꺼내 표시 (클라이언트별 로컬)
 */
UCLASS()
class RETRIEVE_API USystemMessageSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI|System")
	void RequestMessageById(FName RowName);

	UFUNCTION(BlueprintCallable, Category = "Retrieve|UI|System")
	void RequestMessageByKey(FGameplayTag KeyTag);

	bool HasPending() const { return Queue.Num() > 0; }
	bool DequeueNext(FSystemMessageEntry& OutEntry);
	void RequeueFront(const FSystemMessageEntry& Entry);
	FSimpleMulticastDelegate& OnQueued() { return OnQueuedDelegate; }

	// ---- UWorldSubsystem ----
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

protected:
	/** 원시 경로: 채널 broadcast를 받아 텍스트만 큐잉(자격 검사, 1회 표시 없음). */
	void HandleRawMessage(FGameplayTag Channel, const FRetrieveSystemMessagePayload& Message);

	/** 원시 경로 적재(행 이름 없음). */
	void EnqueueRaw(const FText& Text, float Duration);

	/** 공통 적재 */
	void Enqueue(FSystemMessageEntry&& Entry);

	bool IsRowEligible(FName RowName, const FSystemMessageRow& Row) const;
	bool IsRowAlreadyQueued(FName RowName) const;
	void EnqueueRow(FName RowName, const FSystemMessageRow& Row);
	const UDataTable* GetSystemMessageTable() const;
	bool IsStepCompleted(FGameplayTag StepTag) const;

private:
	TArray<FSystemMessageEntry> Queue;

	/** 이번 세션에 이미 1회 표시한 행. 게임 재시작 시 저장되지 않습니다. */
	TSet<FName> PlayedOnceRows;

	TMap<FName, double> LastShownTime;

	FGameplayMessageListenerHandle SystemMessageHandle;
	FSimpleMulticastDelegate OnQueuedDelegate;

	/** 큐 상한. 넘치면 가장 오래된 대기 항목부터 버립니다. */
	int32 MaxQueued = 5;
};
