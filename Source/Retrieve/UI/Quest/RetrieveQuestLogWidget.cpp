#include "RetrieveQuestLogWidget.h"

#include "MVVMSubsystem.h"
#include "Core/RetrieveGameState.h"
#include "Components/ListView.h"
#include "Player/RetrievePlayerController.h"
#include "UI/ViewModels/HUDViewModel.h"
#include "UI/ViewModels/QuestLogViewModel.h"
#include "UI/ViewModels/QuestTrackerViewModel.h"
#include "UI/ViewModels/QuestEntryViewModel.h"
#include "UI/ViewModels/QuestObjectiveEntryViewModel.h"
#include "View/MVVMView.h"

void URetrieveQuestLogWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ARetrievePlayerController* PC = Cast<ARetrievePlayerController>(GetOwningPlayer());
	if (!PC)
	{
		return;
	}

	// PC → HUDViewModel → QuestTracker
	UQuestTrackerViewModel* TrackerVM = nullptr;
	if (UHUDViewModel* HUDVM = PC->GetHUDViewModel())
	{
		TrackerVM = HUDVM->GetQuestTracker();
	}

	if (!LogViewModel)
	{
		LogViewModel = NewObject<UQuestLogViewModel>(this);
	}

	if (UMVVMSubsystem* MVVM = GEngine ? GEngine->GetEngineSubsystem<UMVVMSubsystem>() : nullptr)
	{
		if (UMVVMView* View = MVVM->GetViewFromUserWidget(this))
		{
			View->SetViewModel(TEXT("QuestLog"), LogViewModel);
		}
	}

	LogViewModel->OnListsChanged.AddDynamic(this, &URetrieveQuestLogWidget::HandleListsChanged);

	// 행 클릭 → SelectQuest 연결은 W_QuestLog BP 그래프에서 ListView "On Item Clicked"로 한다
	if (ARetrieveGameState* GS = GetWorld() ? GetWorld()->GetGameState<ARetrieveGameState>() : nullptr)
	{
		LogViewModel->InitializeFromGameState(GS, TrackerVM);
	}
	RefreshLists();
}

void URetrieveQuestLogWidget::NativeDestruct()
{
	if (LogViewModel)
	{
		LogViewModel->OnListsChanged.RemoveDynamic(this, &URetrieveQuestLogWidget::HandleListsChanged);
		LogViewModel->Deinitialize();
	}
	Super::NativeDestruct();
}

void URetrieveQuestLogWidget::HandleListsChanged()
{
	RefreshLists();
}

void URetrieveQuestLogWidget::RefreshLists()
{
	if (!LogViewModel)
	{
		return;
	}
	if (ActiveQuestList)
	{
		ActiveQuestList->SetListItems(LogViewModel->GetActiveQuests());
	}
	if (CompletedQuestList)
	{
		CompletedQuestList->SetListItems(LogViewModel->GetCompletedQuests());
	}
	if (ObjectiveList)
	{
		ObjectiveList->SetListItems(LogViewModel->GetSelectedObjectives());
	}
}
