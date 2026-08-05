#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "RetrieveQuestTrackerWidget.generated.h"

class UQuestTrackerViewModel;
struct FRetrieveQuestStepPayload;
struct FRetrieveObjectiveReminderPayload;

/**
 * HUD 퀘스트 트래커.
 *
 * 목표가 넘어가는 순간과 "목표 재확인" 요청 때 강조 연출을 재생한다.
 * 텍스트만 조용히 바뀌면 플레이어가 진행됐다는 걸 눈치채지 못한다는
 * 유저테스트 피드백에 대한 대응이다.
 */
UCLASS()
class RETRIEVE_API URetrieveQuestTrackerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/**
	 * 강조 시 재생할 위젯 애니메이션 이름.
	 * WBP_QuestTracker에 같은 이름의 애니메이션이 있으면 자동으로 재생된다(없으면 무시).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Quest|Feedback")
	FName HighlightAnimationName = TEXT("Highlight");

	/** 강조 시 함께 재생할 사운드(선택). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Quest|Feedback")
	TObjectPtr<USoundBase> HighlightSound;

	/** 목표가 바뀌었거나 재확인 요청이 왔을 때 호출. BP에서 추가 연출을 붙일 수 있다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|Quest|Feedback")
	void OnObjectiveHighlighted();

	// ── 의뢰(인카운터 퀘스트) 한 줄 ───────────────────────────────────────────
	// 메인 퀘스트 아래에 "지금 받아둔 의뢰 중 가장 가까운 것" 하나만 보여준다.
	// 화면 마커에서 세운 "상시 표시는 하나"라는 규칙을 트래커에서도 유지한다.

	/** 의뢰 구획 전체(제목+목표를 담은 컨테이너). 표시할 의뢰가 없으면 숨겨진다. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UWidget> Box_SideQuest;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> Text_SideQuestTitle;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> Text_SideQuestObjective;

	/** 의뢰 제목 앞에 붙는 말머리. 메인과 텍스트로도 구분된다. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Quest|SideQuest")
	FText SideQuestPrefix = NSLOCTEXT("Retrieve.QuestTracker", "SidePrefix", "의뢰 · ");

	/** 의뢰 줄 색(마커의 인스턴스 색 계열과 맞춘다). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Quest|SideQuest")
	FLinearColor SideQuestColor = FLinearColor(1.0f, 0.62f, 0.35f, 1.0f);

	/** 표시 대상을 다시 고르는 주기(초). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Quest|SideQuest", meta = (ClampMin = "0.05"))
	float SideQuestUpdateInterval = 0.35f;

	/**
	 * 표시 중인 의뢰를 교체하는 기준.
	 * 다른 의뢰가 현재 것보다 이 비율 이상 가까워야 교체한다(0.85 = 15% 이상 가까울 때).
	 * 이게 없으면 두 의뢰가 비슷한 거리일 때 트래커 줄이 계속 깜빡인다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Quest|SideQuest", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float SideQuestSwitchRatio = 0.85f;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	void PlayHighlight();

	void HandleStepChanged(FGameplayTag Channel, const FRetrieveQuestStepPayload& Message);
	void HandleObjectiveReminder(FGameplayTag Channel, const FRetrieveObjectiveReminderPayload& Message);

	/** 가장 가까운 수락 의뢰를 골라 의뢰 줄을 갱신한다(주기 호출). */
	void UpdateSideQuestLine();

private:
	FGameplayMessageListenerHandle StepChangedHandle;
	FGameplayMessageListenerHandle ReminderHandle;

	/** 현재 의뢰 줄에 표시 중인 마커. 히스테리시스 판정의 기준점. */
	FName ShownSideQuestMarkerId;

	FTimerHandle SideQuestTimerHandle;
};
