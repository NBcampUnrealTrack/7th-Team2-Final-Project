#include "QuestEntryViewModel.h"

void UQuestEntryViewModel::SetData(FGameplayTag InQuestId, const FText& InDisplayName, EQuestType InType,
                                   bool bInCompleted, bool bInTracked)
{
	QuestId = InQuestId;
	DisplayName = InDisplayName;
	QuestType = InType;
	bCompleted = bInCompleted;
	bTracked = bInTracked;

	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetQuestId);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetDisplayName);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetQuestType);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetIsCompleted);
	UE_MVVM_BROADCAST_FIELD_VALUE_CHANGED(GetIsTracked);
}
