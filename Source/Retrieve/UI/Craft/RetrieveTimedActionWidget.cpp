#include "UI/Craft/RetrieveTimedActionWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void URetrieveTimedActionWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ProgressBar_32 = Cast<UProgressBar>(GetWidgetFromName(TEXT("ProgressBar_32")));
}

void URetrieveTimedActionWidget::StartTimedAction(float Duration, const FText& ActionText, FSimpleDelegate OnComplete)
{
	TotalDuration = FMath::Max(0.01f, Duration);
	ElapsedTime = 0.0f;
	bIsRunning = true;
	CompletionCallback = MoveTemp(OnComplete);

	if (TXT_Action)
	{
		TXT_Action->SetText(ActionText);
	}
	if (ProgressBar_32)
	{
		ProgressBar_32->SetPercent(0.0f);
	}
}

void URetrieveTimedActionWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bIsRunning)
	{
		return;
	}

	ElapsedTime += InDeltaTime;
	const float Percent = FMath::Clamp(ElapsedTime / TotalDuration, 0.0f, 1.0f);
	if (ProgressBar_32)
	{
		ProgressBar_32->SetPercent(Percent);
	}

	if (ElapsedTime >= TotalDuration)
	{
		bIsRunning = false;
		CompletionCallback.ExecuteIfBound();
	}
}
