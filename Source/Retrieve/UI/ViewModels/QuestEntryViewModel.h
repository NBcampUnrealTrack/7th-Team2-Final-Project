#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "MVVMViewModelBase.h"
#include "Data/RetrieveDataTableTypes.h"
#include "QuestEntryViewModel.generated.h"

/** 퀘스트 로그의 좌측 목록 중 하나의 행. */
UCLASS()
class RETRIEVE_API UQuestEntryViewModel : public UMVVMViewModelBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Quest")
	FGameplayTag GetQuestId() const { return QuestId; }

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Quest")
	FText GetDisplayName() const { return DisplayName; }

	/** Main/Side 구분을 위해 엔트리 위젯이 프레임/아이콘/색 결정에 사용합니다. */
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Quest")
	EQuestType GetQuestType() const { return QuestType; }

	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Quest")
	bool GetIsCompleted() const { return bCompleted; }

	/** ◇ 추적 마커 */
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Quest")
	bool GetIsTracked() const { return bTracked; }

	/**
	 * 인카운터(의뢰) 행이면 그 마커 ID. DT_Quest 행이 아니라 QuestId가 비어 있으므로
	 * 이 값으로 어떤 의뢰인지 식별한다. 일반 퀘스트 행에서는 NAME_None.
	 */
	UFUNCTION(BlueprintPure, FieldNotify, Category = "Retrieve|Quest")
	FName GetEncounterMarkerId() const { return EncounterMarkerId; }

	/** 로그 VM이 행을 채울 때 호출 (전 필드 브로드캐스트) */
	void SetData(FGameplayTag InQuestId, const FText& InDisplayName, EQuestType InType, bool bInCompleted,
	             bool bInTracked);

	/** 인카운터(의뢰) 행 전용 채우기. */
	void SetEncounterData(FName InMarkerId, const FText& InDisplayName, bool bInReadyToTurnIn);

private:
	FName EncounterMarkerId;
	FGameplayTag QuestId;
	FText DisplayName;
	EQuestType QuestType = EQuestType::Main;
	bool bCompleted = false;
	bool bTracked = false;
};
