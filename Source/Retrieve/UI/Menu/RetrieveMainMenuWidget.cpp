#include "RetrieveMainMenuWidget.h"

#include "Components/Button.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Player/RetrievePlayerController.h"
#include "Save/RetrieveSaveSubsystem.h"
#include "Settings/RetrieveGameUserSettings.h"
#include "UI/Sound/RetrieveUISoundTypes.h"

void URetrieveMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);

	if (NewGameButton)
	{
		NewGameButton->OnClicked.AddDynamic(this, &URetrieveMainMenuWidget::HandleNewGameClicked);
	}
	if (ContinueButton)
	{
		ContinueButton->OnClicked.AddDynamic(this, &URetrieveMainMenuWidget::HandleContinueClicked);
	}
	if (LoadGameButton)
	{
		LoadGameButton->OnClicked.AddDynamic(this, &URetrieveMainMenuWidget::HandleLoadGameClicked);
	}
	if (OptionsButton)
	{
		OptionsButton->OnClicked.AddDynamic(this, &URetrieveMainMenuWidget::HandleOptionsClicked);
	}
	if (CreditsButton)
	{
		CreditsButton->OnClicked.AddDynamic(this, &URetrieveMainMenuWidget::HandleCreditsClicked);
	}
	if (QuitButton)
	{
		QuitButton->OnClicked.AddDynamic(this, &URetrieveMainMenuWidget::HandleQuitClicked);
	}

	RegisterSoundButton(NewGameButton);
	RegisterSoundButton(ContinueButton);
	RegisterSoundButton(LoadGameButton);
	RegisterSoundButton(OptionsButton);
	RegisterSoundButton(CreditsButton);
	RegisterSoundButton(QuitButton);

	bSaveDataExists = false;
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const URetrieveSaveSubsystem* SaveSubsystem = GI->GetSubsystem<URetrieveSaveSubsystem>())
		{
			bSaveDataExists = SaveSubsystem->HasSaveGame();
		}
	}

	if (ContinueButton)
	{
		ContinueButton->SetIsEnabled(bSaveDataExists);
	}
	if (LoadGameButton)
	{
		LoadGameButton->SetIsEnabled(bSaveDataExists);
	}

	ShowTitleCard();
}

FReply URetrieveMainMenuWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (!bTitleCardDismissed)
	{
		bTitleCardDismissed = true;
		PlayUISound(ERetrieveUISoundEvent::PanelOpen);
		PlayTitleDismissTransition();
		return FReply::Handled();
	}

	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		ShowTitleCard();
		return FReply::Handled();
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

FReply URetrieveMainMenuWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (!bTitleCardDismissed)
	{
		bTitleCardDismissed = true;
		PlayUISound(ERetrieveUISoundEvent::PanelOpen);
		PlayTitleDismissTransition();
		return FReply::Handled();
	}

	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

void URetrieveMainMenuWidget::HandleNewGameClicked()
{
	// TODO: 메뉴 관리 세이브 슬롯이 추가되면 bSaveDataExists가 true일 때 시작 전 "기존 세이브를 덮어씁니다" 확인 팝업 표시
	// 현재는 새 게임이 세이브 슬롯을 삭제하지 않음

	if (ARetrievePlayerController* PC = GetRetrievePlayerController())
	{
		PC->RequestNewGame();
	}
}

void URetrieveMainMenuWidget::HandleContinueClicked()
{
	if (!bSaveDataExists)
	{
		return;
	}

	if (ARetrievePlayerController* PC = GetRetrievePlayerController())
	{
		PC->RequestContinueGame();
	}
}

void URetrieveMainMenuWidget::HandleLoadGameClicked()
{
	if (!bSaveDataExists)
	{
		return;
	}

	if (ARetrievePlayerController* PC = GetRetrievePlayerController())
	{
		PC->OpenLoadGamePanel();
	}
}

void URetrieveMainMenuWidget::HandleOptionsClicked()
{
	if (ARetrievePlayerController* PC = GetRetrievePlayerController())
	{
		PC->OpenSettingsPanel();
	}
}

void URetrieveMainMenuWidget::HandleCreditsClicked()
{
	if (ARetrievePlayerController* PC = GetRetrievePlayerController())
	{
		PC->OpenCreditsPanel();
	}
}

void URetrieveMainMenuWidget::HandleQuitClicked()
{
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void URetrieveMainMenuWidget::ShowTitleCard()
{
	bTitleCardDismissed = false;

	if (MenuPaneRoot)
	{
		MenuPaneRoot->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (TitleCardRoot)
	{
		TitleCardRoot->SetVisibility(ESlateVisibility::Visible);
	}

	FocusNextTick(this);
	OnTitleCardShown();
}

void URetrieveMainMenuWidget::ShowMenuPane()
{
	bTitleCardDismissed = true;

	if (TitleCardRoot)
	{
		TitleCardRoot->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (MenuPaneRoot)
	{
		MenuPaneRoot->SetVisibility(ESlateVisibility::Visible);
	}

	FocusNextTick(NewGameButton ? static_cast<UWidget*>(NewGameButton) : static_cast<UWidget*>(this));
	OnMenuPaneShown();
}

void URetrieveMainMenuWidget::PlayTitleDismissTransition_Implementation()
{
	ShowMenuPane();
}

void URetrieveMainMenuWidget::FinishTitleTransition()
{
	ShowMenuPane();
}

bool URetrieveMainMenuWidget::IsReduceMotionEnabled() const
{
	const URetrieveGameUserSettings* Settings = URetrieveGameUserSettings::Get();
	return Settings && Settings->bReduceMotion;
}

void URetrieveMainMenuWidget::FocusNextTick(TWeakObjectPtr<UWidget> Target)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick([Target]()
		{
			if (UWidget* Widget = Target.Get())
			{
				Widget->SetKeyboardFocus();
			}
		});
	}
}

ARetrievePlayerController* URetrieveMainMenuWidget::GetRetrievePlayerController() const
{
	return Cast<ARetrievePlayerController>(GetOwningPlayer());
}
