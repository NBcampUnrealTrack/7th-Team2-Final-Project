#include "RetrieveQuestLogEntryWidget.h"

#include "MVVMSubsystem.h"
#include "UI/ViewModels/QuestEntryViewModel.h"
#include "View/MVVMView.h"

void URetrieveQuestLogEntryWidget::NativeOnListItemObjectSet(UObject* ListItemObject)
{
	UQuestEntryViewModel* EntryVM = Cast<UQuestEntryViewModel>(ListItemObject);
	if (!EntryVM)
	{
		return;
	}

	if (UMVVMSubsystem* MVVM = GEngine ? GEngine->GetEngineSubsystem<UMVVMSubsystem>() : nullptr)
	{
		if (UMVVMView* View = MVVM->GetViewFromUserWidget(this))
		{
			View->SetViewModel(TEXT("QuestEntry"), EntryVM);
		}
	}

	FRetrieveQuestTypeStyle Style;
	if (StyleAsset)
	{
		StyleAsset->GetStyle(EntryVM->GetQuestType(), Style);
	}
	ApplyQuestTypeStyle(Style, EntryVM->GetIsTracked(), EntryVM->GetIsCompleted());
}
