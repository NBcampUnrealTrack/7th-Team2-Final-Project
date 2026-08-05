#include "RetrieveLoadingScreenWidget.h"

#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Subsystems/RetrieveGuidanceSubsystem.h"

void URetrieveLoadingScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 현재 목표 한 줄. 레벨 전환 중이라 월드가 없어도 GameInstance 캐시에서 읽어온다.
	if (Text_Objective)
	{
		FText Brief;
		if (const UGameInstance* GI = GetGameInstance())
		{
			if (const URetrieveGuidanceSubsystem* Guidance = GI->GetSubsystem<URetrieveGuidanceSubsystem>())
			{
				Brief = Guidance->GetObjectiveBriefLine();
			}
		}

		Text_Objective->SetText(Brief);
		// 첫 로딩(아직 목표 없음)에는 빈 줄이 뜨지 않게 접는다.
		Text_Objective->SetVisibility(
			Brief.IsEmptyOrWhitespace() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}

	if (Text_Tip)
	{
		if (Tips.Num() > 0)
		{
			Text_Tip->SetText(Tips[FMath::RandRange(0, Tips.Num() - 1)]);
			Text_Tip->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Text_Tip->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void URetrieveLoadingScreenWidget::PlayFadeOutAndRemove()
{
	if (bFadeOutStarted)
	{
		return;
	}
	bFadeOutStarted = true;

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
