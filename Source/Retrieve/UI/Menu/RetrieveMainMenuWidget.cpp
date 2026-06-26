#include "RetrieveMainMenuWidget.h"

#include "Components/Button.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Player/RetrievePlayerController.h"
#include "Save/RetrieveSaveSubsystem.h"

void URetrieveMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// "아무 키나 누르세요" + 화살표 내비게이션을 위한 키보드 입력 받기
	SetIsFocusable(true);

	if (NewGameButton)
	{
		NewGameButton->OnClicked.AddDynamic(this, &URetrieveMainMenuWidget::HandleNewGameClicked);
	}
	if (QuitButton)
	{
		QuitButton->OnClicked.AddDynamic(this, &URetrieveMainMenuWidget::HandleQuitClicked);
	}

	// TODO: 표시는 되지만 상호작용은 안됨
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
		ContinueButton->SetIsEnabled(false);
	}
	if (LoadGameButton)
	{
		LoadGameButton->SetIsEnabled(false);
	}
	if (OptionsButton)
	{
		OptionsButton->SetIsEnabled(false);
	}
	if (CreditsButton)
	{
		CreditsButton->SetIsEnabled(false);
	}

	ShowTitleCard();
}

FReply URetrieveMainMenuWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (!bTitleCardDismissed)
	{
		DismissTitleCard();
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
		DismissTitleCard();
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

	// 다음 키나 클릭이 위의 preview 핸들러에서 잡힐 수 있도록 루트에 포커스 유지
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

	// 첫 번째 동작 항목에 즉시 키보드 선택 포커스
	FocusNextTick(NewGameButton ? static_cast<UWidget*>(NewGameButton) : static_cast<UWidget*>(this));
	OnMenuPaneShown();
}

void URetrieveMainMenuWidget::DismissTitleCard()
{
	ShowMenuPane();
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
