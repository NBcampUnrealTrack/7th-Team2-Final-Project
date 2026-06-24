#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "Data/RetrieveDataTableTypes.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Components/SlateWrapperTypes.h"

#include "QuestTrackerViewModel.generated.h"

class ARetrieveGameState;

/**
 * W_QuestTracker(HUD)를 구동하는 ViewModel.
 * 퀘스트 상태를 들고 있지 않는다. 대신 다음 두 곳을 읽어서
 * "지금 추적 중인 퀘스트와 그 미완료 목표"를 그때그때 계산한다:
 *   - UQuestBranchComponent의 CompletedSteps : 어떤 스텝이 끝났는지(진행 기록)
 *   - DT_Quest : 퀘스트 이름 / 목표 / 타입 등 정의
 *   
 * InitializeFromGameState 시 현재 상태로 한 번 계산하고,
 * 이후 Channel.Quest.StepChanged 메시지가 올 때마다 다시 계산(Recompute)한다.
 */
UCLASS()
class RETRIEVE_API UQuestTrackerViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Quest")
	FText GetQuestName() const { return QuestName; }

	/** Main/Side 타입 아이콘 분기용. */
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Quest")
	EQuestType GetQuestType() const { return QuestType; }

	/** 추적 퀘스트의 미완료 목표만 표시 순서대로, MaxTrackedObjectives개로 제한. */
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Quest")
	TArray<FText> GetObjectives() const { return Objectives; }
	
	/** 미완료 목표를 줄바꿈으로 결합(멀티라인 TextBlock 하나에 바인딩). */
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Quest")
	FText GetObjectivesText() const;

	/** 추적 중인 활성 퀘스트가 없거나 시네마틱 중이면 false. */
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Quest")
	bool GetIsVisible() const { return bVisible; }

	void InitializeFromGameState(ARetrieveGameState* GameState);
	void Deinitialize();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Quest")
	void SetTrackedQuest(FGameplayTag QuestId);
	
	// ── 고정 3행 트래커용 인덱스 접근자(범위 밖이면 빈 FText → 그 행은 숨김) ──
	// TODO: RichText/ListView로 대체하는 것 고려
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Quest")
	FText GetObjective0() const { return Objectives.IsValidIndex(0) ? Objectives[0] : FText::GetEmpty(); }

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Quest")
	FText GetObjective1() const { return Objectives.IsValidIndex(1) ? Objectives[1] : FText::GetEmpty(); }

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Quest")
	
	FText GetObjective2() const { return Objectives.IsValidIndex(2) ? Objectives[2] : FText::GetEmpty(); }
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Quest")
	ESlateVisibility GetObjective0Visibility() const { return Objectives.IsValidIndex(0) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed; }

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Quest")
	ESlateVisibility GetObjective1Visibility() const { return Objectives.IsValidIndex(1) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed; }

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Quest")
	ESlateVisibility GetObjective2Visibility() const { return Objectives.IsValidIndex(2) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed; }

protected:
	virtual void BeginDestroy() override;

private:
	void HandleStepChanged(FGameplayTag Channel, const FRetrieveQuestStepPayload& Message);
	void HandleCinematicChanged(FGameplayTag Channel, const FRetrieveCinematicStatePayload& Message);
	void Recompute();
	void BroadcastAllFields();

	static constexpr int32 MaxTrackedObjectives = 3;

	FGameplayMessageListenerHandle StepListenerHandle;
	FGameplayMessageListenerHandle CinematicListenerHandle;
	TWeakObjectPtr<UWorld> WorldPtr;

	/** 로컬에서 선택한 퀘스트 추적. (기본값: DisplayOrder 순 첫 Active 메인 퀘스트) */
	FGameplayTag TrackedQuestId;

	UPROPERTY()
	FText QuestName;

	EQuestType QuestType = EQuestType::Main;

	UPROPERTY()
	TArray<FText> Objectives;

	bool bVisible = false;
	bool bCinematicActive = false;
};
