#pragma once

#include "CoreMinimal.h"
#include "UI/RetrieveGamePanelWidget.h"
#include "RetrieveQuestLogWidget.generated.h"

class UListView;
class UQuestLogViewModel;

/** 2-pane 퀘스트 로그 모달. 패널 시스템(OpenExclusivePanel)으로 열립니다. */
UCLASS()
class RETRIEVE_API URetrieveQuestLogWidget : public URetrieveGamePanelWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// UMG의 동일 이름 위젯과 바인딩. 좌측 두 섹션 + 우측 목표 목록.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UListView> ActiveQuestList;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UListView> CompletedQuestList;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UListView> ObjectiveList;

	UPROPERTY()
	TObjectPtr<UQuestLogViewModel> LogViewModel;

	UFUNCTION()
	void HandleListsChanged();

	/** BP 그래프에서 ListView "On Item Clicked" → SelectQuest 연결용 */
	UFUNCTION(BlueprintPure, Category = "Retrieve|Quest")
	UQuestLogViewModel* GetLogViewModel() const { return LogViewModel; }

private:
	void RefreshLists();
};
