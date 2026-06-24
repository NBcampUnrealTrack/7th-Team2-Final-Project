#include "UI/HUD/RetrieveQuestTrackerWidget.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "MVVMSubsystem.h"
#include "View/MVVMView.h"

#include "Core/RetrieveGameState.h"
#include "Player/RetrievePlayerController.h"
#include "UI/ViewModels/HUDViewModel.h"
#include "UI/ViewModels/QuestTrackerViewModel.h"

void URetrieveQuestTrackerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ARetrievePlayerController* PlayerController = Cast<ARetrievePlayerController>(GetOwningPlayer());
	if (!PlayerController)
	{
		return;
	}

	UHUDViewModel* HUDVM = PlayerController->GetHUDViewModel();
	if (!HUDVM)
	{
		return;
	}

	UQuestTrackerViewModel* TrackerVM = HUDVM->GetQuestTracker();
	if (!TrackerVM)
	{
		return;
	}

	if (UMVVMSubsystem* MVVM = GEngine ? GEngine->GetEngineSubsystem<UMVVMSubsystem>() : nullptr)
	{
		if (UMVVMView* View = MVVM->GetViewFromUserWidget(this))
		{
			View->SetViewModel(TEXT("QuestTracker"), TrackerVM);
		}
	}

	if (ARetrieveGameState* GS = GetWorld() ? GetWorld()->GetGameState<ARetrieveGameState>() : nullptr)
	{
		TrackerVM->InitializeFromGameState(GS);
	}
}
