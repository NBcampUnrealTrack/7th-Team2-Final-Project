#include "QuestEntryViewModel.h"

void UQuestEntryViewModel::SetData(FGameplayTag InQuestId, const FText& InDisplayName, EQuestType InType,
                                   bool bInCompleted, bool bInTracked)
{
	EncounterMarkerId = NAME_None;
	QuestId = InQuestId;
	DisplayName = InDisplayName;
	QuestType = InType;
	bCompleted = bInCompleted;
	bTracked = bInTracked;

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetEncounterMarkerId);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetQuestId);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetDisplayName);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetQuestType);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetIsCompleted);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetIsTracked);
}

void UQuestEntryViewModel::SetEncounterData(FName InMarkerId, const FText& InDisplayName, bool bInReadyToTurnIn)
{
	EncounterMarkerId = InMarkerId;
	QuestId = FGameplayTag(); // DT_Quest 행이 아니므로 비운다.
	DisplayName = InDisplayName;
	QuestType = EQuestType::Side; // 엔트리 위젯의 Side 프레임/색 분기를 그대로 재사용한다.
	bCompleted = false;           // 목록상 "진행 중"이며, 보상 대기도 아직 완료가 아니다.
	bTracked = bInReadyToTurnIn;  // 보상 받으러 갈 차례면 ◇로 눈에 띄게 한다.

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetEncounterMarkerId);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetQuestId);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetDisplayName);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetQuestType);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetIsCompleted);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetIsTracked);
}
