#pragma once

#include "CoreMinimal.h"
#include "MVVMViewModelBase.h"
#include "QuestObjectiveEntryViewModel.generated.h"

/** 퀘스트 로그의 우측 상세 내용 중 하나의 목표. */
UCLASS()
class RETRIEVE_API UQuestObjectiveEntryViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Quest")
	FText GetObjectiveText() const { return ObjectiveText; }

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Quest")
	bool GetIsCompleted() const { return bCompleted; }

	void SetData(const FText& InText, bool bInCompleted);

private:
	FText ObjectiveText;
	bool bCompleted = false;
};
