#include "RetrieveLoadingScreenWidget.h"

void URetrieveLoadingScreenWidget::PlayFadeOutAndRemove()
{
	if (FadeOutAnim)
	{
		FWidgetAnimationDynamicEvent EndDelegate;
		EndDelegate.BindDynamic(this, &URetrieveLoadingScreenWidget::HandleFadeOutFinished);
		BindToAnimationFinished(FadeOutAnim, EndDelegate);
		PlayAnimation(FadeOutAnim);
	}
	else
	{
		RemoveFromParent();
		OnRemoved.Broadcast();
	}
}

void URetrieveLoadingScreenWidget::HandleFadeOutFinished()
{
	RemoveFromParent();
	OnRemoved.Broadcast();
}
