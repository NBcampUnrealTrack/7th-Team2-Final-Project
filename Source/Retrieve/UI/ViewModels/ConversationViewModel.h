#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Messaging/RetrieveMessageTypes.h"
#include "ConversationViewModel.generated.h"

/**
 * 대화 뷰(W_Conversation)를 구동하는 ViewModel.
 * 비트 하나 = Lines[] (한 번에 하나씩 Enter/클릭으로 로컬 진행) -> Topics[] (마지막 줄 이후 표시).
 */
UCLASS(BlueprintType)
class RETRIEVE_API UConversationViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	// ── 바인딩 가능한 인터페이스 (W_Conversation이 바인딩함) ───────────────────────────────
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Dialogue")
	FText GetSpeakerName() const { return SpeakerName; }

	/** 현재 표시 중인 라인. */
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Dialogue")
	FText GetCurrentLine() const { return Lines.IsValidIndex(LineIndex) ? Lines[LineIndex] : FText::GetEmpty(); }

	/** 비트의 마지막 라인에 도달(또는 라인이 없을 때)까지 토픽은 숨겨집니다. */
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Dialogue")
	bool GetShowChoices() const { return Lines.Num() == 0 || LineIndex >= Lines.Num() - 1; }

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Dialogue")
	TArray<FRetrieveDialogueTopic> GetTopics() const { return Topics; }

	/** 표시 중인 토픽 개수(숫자키 유효성 판단·번호 라벨 표시용). */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Dialogue")
	int32 GetTopicCount() const { return Topics.Num(); }

	/**  비호스트는 false. 공유되는 대화에서 선택은 호스트만 가능합니다. */
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Dialogue")
	bool GetTopicsEnabled() const { return bTopicsEnabled; }

	// ── 라이프사이클 (ARetrievePlayerController::Client_OpenConversation / CloseConversation이 호출) ──
	/** Channel.Dialogue.LineRequested를 구독하고, 현재 Replicate 된 DialogueState로 초기화합니다. */
	void Initialize(UWorld* World, APlayerController* InOwningPlayerController);

	/** 리스너 구독을 해제합니다. */
	void Deinitialize();

	/** GameState를 경유하는 DT_Dialogue와, Replicate된 CompletedSteps를 읽어 SpeakerName / Lines / Topics를 채웁니다.
	 * Initialize 직후 한 번 호출됩니다. */
	void BuildOpeningTopicsFor(AActor* NPC);

	// ── 플레이어 액션 ──────────────────────────────────────────────────────────────────
	/** Enter 또는 클릭으로 다음 라인으로 진행합니다. 선택지가 표시되면 무시됩니다. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Dialogue")
	void Advance();

	/** 토픽 버튼, 또는 무효 TopicId를 가진 ESC/대화 종료(Goodbye) */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Dialogue")
	void OnTopicSelected(FGameplayTag TopicId);

	/** 인덱스로 토픽 선택(숫자키/방향키+Enter용). 유효·활성 검사 후 OnTopicSelected로 위임. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Dialogue")
	void SelectTopicByIndex(int32 Index);

protected:
	virtual void BeginDestroy() override;

private:
	void HandleLineRequested(FGameplayTag Channel, const FRetrieveDialoguePayload& Message);
	void AppendGoodbyeIfMissing();
	void BroadcastBeatFields();

	FGameplayMessageListenerHandle ListenerHandle;
	TWeakObjectPtr<UWorld> WorldPtr;
	TWeakObjectPtr<APlayerController> OwningPlayerController;

	UPROPERTY()
	FText SpeakerName;

	UPROPERTY()
	TArray<FText> Lines;

	UPROPERTY()
	TArray<FRetrieveDialogueTopic> Topics;

	int32 LineIndex = 0;
	bool bTopicsEnabled = true;
};
