#include "UI/Menu/RetrieveSystemMenuWidget.h"
#include "Player/RetrievePlayerController.h"
#include "Components/Button.h"
#include "Kismet/KismetSystemLibrary.h"

void URetrieveSystemMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (ResumeButton && !ResumeButton->OnClicked.IsAlreadyBound(this, &ThisClass::HandleResumeClicked))
	{
		ResumeButton->OnClicked.AddDynamic(this, &ThisClass::HandleResumeClicked);
	}
	if (SettingsButton && !SettingsButton->OnClicked.IsAlreadyBound(this, &ThisClass::HandleSettingsClicked))
	{
		SettingsButton->OnClicked.AddDynamic(this, &ThisClass::HandleSettingsClicked);
	}
	if (CommandsButton && !CommandsButton->OnClicked.IsAlreadyBound(this, &ThisClass::HandleCommandsClicked))
	{
		CommandsButton->OnClicked.AddDynamic(this, &ThisClass::HandleCommandsClicked);
	}
	if (MainMenuButton && !MainMenuButton->OnClicked.IsAlreadyBound(this, &ThisClass::HandleMainMenuClicked))
	{
		MainMenuButton->OnClicked.AddDynamic(this, &ThisClass::HandleMainMenuClicked);
	}
	if (QuitButton && !QuitButton->OnClicked.IsAlreadyBound(this, &ThisClass::HandleQuitClicked))
	{
		QuitButton->OnClicked.AddDynamic(this, &ThisClass::HandleQuitClicked);
	}
}

void URetrieveSystemMenuWidget::HandleResumeClicked()
{
	// 패널을 닫아 게임으로 복귀.
	RequestClose();
}

void URetrieveSystemMenuWidget::HandleSettingsClicked()
{
	if (ARetrievePlayerController* PC = Cast<ARetrievePlayerController>(GetOwningPlayer()))
	{
		// OpenSettingsPanel → OpenExclusivePanel이 현재 시스템 메뉴 패널을 설정 화면으로 교체한다.
		PC->OpenSettingsPanel();
	}
}

void URetrieveSystemMenuWidget::HandleCommandsClicked()
{
	if (ARetrievePlayerController* PC = Cast<ARetrievePlayerController>(GetOwningPlayer()))
	{
		// OpenControlsGuide → OpenExclusivePanel이 현재 시스템 메뉴 패널을 조작키 안내 화면으로 교체한다.
		PC->OpenControlsGuide();
	}
}

void URetrieveSystemMenuWidget::HandleMainMenuClicked()
{
	if (ARetrievePlayerController* PC = Cast<ARetrievePlayerController>(GetOwningPlayer()))
	{
		// 서버 권위 상태머신을 MainMenu로 전환(HandleSessionStateChanged가 패널 정리·위젯 스왑 담당).
		PC->Server_RequestQuitToMenu();
	}
}

void URetrieveSystemMenuWidget::HandleQuitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
