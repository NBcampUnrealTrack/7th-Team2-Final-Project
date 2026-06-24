#include "QuestObjectiveEntryViewModel.h"

void UQuestObjectiveEntryViewModel::SetData(const FText& InText, bool bInCompleted)
{
	ObjectiveText = InText;
	bCompleted = bInCompleted;

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetObjectiveText);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetIsCompleted);
}
