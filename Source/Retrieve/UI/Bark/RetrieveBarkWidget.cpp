#include "RetrieveBarkWidget.h"

#include "MVVMSubsystem.h"
#include "RetrieveSubtitleLineWidget.h"
#include "UI/ViewModels/BarkViewModel.h"
#include "View/MVVMView.h"

void URetrieveBarkWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (!BarkViewModel)
	{
		BarkViewModel = NewObject<UBarkViewModel>(this);
	}

	if (UMVVMSubsystem* MVVM = GEngine ? GEngine->GetEngineSubsystem<UMVVMSubsystem>() : nullptr)
	{
		if (UMVVMView* View = MVVM->GetViewFromUserWidget(this))
		{
			View->SetViewModel(TEXT("Bark"), BarkViewModel);
		}
	}

	BarkViewModel->OnShowLine.AddDynamic(this, &URetrieveBarkWidget::HandleShowLine);
	BarkViewModel->OnHideLine.AddDynamic(this, &URetrieveBarkWidget::HandleHideLine);

	BarkViewModel->Initialize(GetWorld(), BarkStyle);
}

void URetrieveBarkWidget::NativeDestruct()
{
	if (BarkViewModel)
	{
		BarkViewModel->OnShowLine.RemoveDynamic(this, &URetrieveBarkWidget::HandleShowLine);
		BarkViewModel->OnHideLine.RemoveDynamic(this, &URetrieveBarkWidget::HandleHideLine);
		BarkViewModel->Shutdown();
	}
	Super::NativeDestruct();
}

void URetrieveBarkWidget::HandleShowLine()
{
	if (SubtitleLine && BarkViewModel)
	{
		SubtitleLine->SetLine(BarkViewModel->GetSpeakerName(), BarkViewModel->GetLineText(),
		                      BarkViewModel->GetNameColor(), BarkViewModel->GetAccentColor());
		SubtitleLine->PlayFadeIn();
	}
}

void URetrieveBarkWidget::HandleHideLine()
{
	if (SubtitleLine)
	{
		SubtitleLine->PlayFadeOut();
	}
}
