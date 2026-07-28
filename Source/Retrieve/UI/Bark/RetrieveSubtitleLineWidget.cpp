#include "RetrieveSubtitleLineWidget.h"

#include "Components/TextBlock.h"

void URetrieveSubtitleLineWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetVisibility(ESlateVisibility::Collapsed); // 표시 전까지 숨김

	if (FadeOutAnim)
	{
		FWidgetAnimationDynamicEvent FinishedEvent;
		FinishedEvent.BindDynamic(this, &URetrieveSubtitleLineWidget::HandleFadeOutFinished);
		BindToAnimationFinished(FadeOutAnim, FinishedEvent); // 페이드아웃 종료 시 Collapsed
	}
}

void URetrieveSubtitleLineWidget::SetLine(const FText& InSpeaker, const FText& InLine, FLinearColor InNameColor,
                                          FLinearColor InAccentColor)
{
	if (SpeakerNameText)
	{
		SpeakerNameText->SetText(InSpeaker);
		SpeakerNameText->SetColorAndOpacity(FSlateColor(InNameColor));
		// 스피커 이름이 없으면 숨김(시네마틱 자막의 경우)
		SpeakerNameText->SetVisibility(InSpeaker.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	if (LineText)
	{
		LineText->SetText(InLine);
		LineText->SetColorAndOpacity(FSlateColor(InAccentColor));
	}
}

void URetrieveSubtitleLineWidget::PlayFadeIn()
{
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (FadeInAnim)
	{
		PlayAnimation(FadeInAnim);
	}
}

void URetrieveSubtitleLineWidget::PlayFadeOut()
{
	if (FadeOutAnim)
	{
		PlayAnimation(FadeOutAnim); // 종료 시 HandleFadeOutFinished → Collapsed
	}
	else
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

void URetrieveSubtitleLineWidget::HandleFadeOutFinished()
{
	SetVisibility(ESlateVisibility::Collapsed);
}
