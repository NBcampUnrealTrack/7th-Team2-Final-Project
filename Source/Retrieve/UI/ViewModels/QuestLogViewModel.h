#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "GameFramework/GameplayMessageSubsystem.h"
#include "Data/RetrieveDataTableTypes.h"
#include "QuestLogViewModel.generated.h"

class ARetrieveGameState;
class UQuestBranchComponent;
class UQuestEntryViewModel;
class UQuestObjectiveEntryViewModel;
class UQuestTrackerViewModel;
struct FQuestDefinition;
struct FRetrieveQuestStepPayload;

/** Recompute 직후 발화 — 위젯이 ListView 아이템을 갱신하도록 합니다. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRetrieveQuestLogListsChangedSignature);

/**
 * W_QuestLog(모달)를 구동하는 ViewModel. 퀘스트 상태를 저장하지 않습니다.
 * - Active/Completed 목록과 선택 퀘스트의 목표를 CompletedSteps + DT_Quest에서 파생.
 * - init 시 현재 상태로 스냅샷, 이후 Channel.Quest.StepChanged마다 재계산 (트래커 VM과 동일 패턴).
 */
UCLASS()
class RETRIEVE_API UQuestLogViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	// ── 좌측의 상하로 나뉜 섹션 ──────────────────────────────────────
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Quest")
	TArray<UQuestEntryViewModel*> GetActiveQuests() const;

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Quest")
	TArray<UQuestEntryViewModel*> GetCompletedQuests() const;

	/** Completed 섹션(구분선 포함)이 비면 숨김. */
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Quest")
	ESlateVisibility GetCompletedSectionVisibility() const;

	// ── 우측(선택한 퀘스트) ────────────────────────────────────────
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Quest")
	FText GetSelectedDisplayName() const { return SelectedDisplayName; }

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Quest")
	FText GetSelectedDescription() const { return SelectedDescription; }

	/** 선택한 퀘스트의 Main/Side 타입 — 우측 제목(Quest Name) 양쪽 프레임 분기용. */
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Quest")
	EQuestType GetSelectedQuestType() const { return SelectedQuestType; }

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Quest")
	TArray<UQuestObjectiveEntryViewModel*> GetSelectedObjectives() const;

	/** 우측 패널 표시 여부(선택한 퀘스트가 있을 때만). */
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Quest")
	bool GetHasSelection() const { return bHasSelection; }

	/** Track 버튼 활성 조건: 선택한 퀘스트가 Active(미완료) 퀘스트이고 아직 추적 중이 아님. */
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Quest")
	bool GetCanTrackSelected() const { return bCanTrackSelected; }

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Quest")
	FRetrieveQuestLogListsChangedSignature OnListsChanged;

	// ── 라이프사이클 (W_QuestLog가 호출) ──────────────────────────
	void InitializeFromGameState(ARetrieveGameState* GameState, UQuestTrackerViewModel* InTracker);
	void Deinitialize();

	// ── 플레이어 액션 ────────────────────────────────────────────
	/** 좌측 행 클릭 → 우측 패널 채움, 상세 내용 표시 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Quest")
	void SelectQuest(FGameplayTag QuestId);

	/**
	 * 의뢰(인카운터) 행 클릭. DT_Quest 행이 없으므로 마커 정보로 우측 패널을 채운다.
	 * 엔트리 위젯은 GetEncounterMarkerId()가 비어 있지 않으면 이쪽을 호출하면 된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Quest")
	void SelectEncounterQuest(FName MarkerId);

	/** Track 버튼 → 기존 트래커 VM 재추적 + 추적 중을 나타내는 로컬 아이콘 ◇ 갱신. */
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Quest")
	void TrackSelectedQuest();

protected:
	virtual void BeginDestroy() override;

private:
	void HandleStepChanged(FGameplayTag Channel, const FRetrieveQuestStepPayload& Message);
	void Recompute();

	/** 수락한 인카운터 퀘스트를 진행 중 목록 뒤에 덧붙인다(가까운 순). */
	void AppendEncounterQuests();

	void RebuildRightPane(const UQuestBranchComponent& Branch, const UDataTable& Table);
	FGameplayTag ResolveTrackedQuestId(const TArray<FQuestDefinition*>& SortedRows, const UQuestBranchComponent& Branch) const;
	const FQuestDefinition* FindQuestRow(const UDataTable& Table, FGameplayTag QuestId) const;
	void BroadcastListFields();

	FGameplayMessageListenerHandle StepListenerHandle;
	TWeakObjectPtr<UWorld> WorldPtr;
	TWeakObjectPtr<UQuestTrackerViewModel> TrackerVM;

	/** 로컬에서 선택한 퀘스트 추적. (기본값: DisplayOrder 순 첫 Active 메인 퀘스트) — 트래커와 동일한 규칙. */
	FGameplayTag TrackedQuestId;
	FGameplayTag SelectedQuestId;

	/** 의뢰 행을 선택한 경우의 마커 ID(일반 퀘스트 선택 시 NAME_None). */
	FName SelectedEncounterMarkerId;

	UPROPERTY()
	TArray<TObjectPtr<UQuestEntryViewModel>> ActiveQuests;

	UPROPERTY()
	TArray<TObjectPtr<UQuestEntryViewModel>> CompletedQuests;

	UPROPERTY()
	TArray<TObjectPtr<UQuestObjectiveEntryViewModel>> SelectedObjectives;

	FText SelectedDisplayName;
	FText SelectedDescription;
	EQuestType SelectedQuestType = EQuestType::Main;
	bool bHasSelection = false;
	bool bCanTrackSelected = false;
};
