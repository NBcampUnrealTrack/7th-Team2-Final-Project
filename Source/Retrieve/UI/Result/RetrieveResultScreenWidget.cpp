#include "RetrieveResultScreenWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Player/RetrievePlayerController.h"

void URetrieveResultScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();


	if (TitleText)
	{
		TitleText->SetText(DeathTitleText);
	}

	// 사망 대기 동안 페이드인 애니메이션 재생
	if (FadeInAnim)
	{
		PlayAnimation(FadeInAnim);
	}

	if (RespawnDelay > 0.f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(RespawnTimerHandle, this, &ThisClass::RequestRespawn, RespawnDelay,
			                                  false);
		}
		else
		{
			RequestRespawn();
		}
	}
	else
	{
		RequestRespawn();
	}
}

void URetrieveResultScreenWidget::RequestRespawn()
{
	if (ARetrievePlayerController* PC = Cast<ARetrievePlayerController>(GetOwningPlayer()))
	{
		PC->Server_RequestRetry();
	}
}
