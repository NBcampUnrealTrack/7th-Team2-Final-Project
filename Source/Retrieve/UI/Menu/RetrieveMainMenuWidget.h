#pragma once

#include "CoreMinimal.h"
#include "UI/VFX/RetrieveUIVFXWidget.h"
#include "RetrieveMainMenuWidget.generated.h"

class UButton;
class ARetrievePlayerController;

/**
 * W_MainMenu의 C++ 베이스 클래스. SessionState == MainMenu일 때 ARetrievePlayerController가 표시합니다.
 * URetrieveUIVFXWidget을 상속하여 UI 사운드/이펙트 라우팅과 Reduce Motion 처리를 그대로 사용합니다.
 */
UCLASS()
class RETRIEVE_API URetrieveMainMenuWidget : public URetrieveUIVFXWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// ---- 섹션 ----
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> TitleCardRoot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UWidget> MenuPaneRoot;

	// ---- 버튼 ----
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> NewGameButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ContinueButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> LoadGameButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> OptionsButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> CreditsButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> QuitButton;

	/** 디스크에 세이브가 존재하는지 여부 (URetrieveSaveSubsystem::HasSaveGame). WBP에서 계속하기/불러오기 서브레이블 표시용 */
	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Menu")
	bool bSaveDataExists = false;

	/** 메뉴 패널이 표시될 때의 훅 (인트로 애니메이션 재생) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|Menu")
	void OnMenuPaneShown();

	/** 타이틀 카드가 복원될 때의 훅 (메뉴 패널에서 Esc) */
	UFUNCTION(BlueprintImplementableEvent, Category = "Retrieve|Menu")
	void OnTitleCardShown();

	UFUNCTION(BlueprintNativeEvent, Category = "Retrieve|Menu")
	void PlayTitleDismissTransition();
	virtual void PlayTitleDismissTransition_Implementation();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Menu")
	void FinishTitleTransition();

	UFUNCTION(BlueprintPure, Category = "Retrieve|Menu")
	bool IsReduceMotionEnabled() const;

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Menu")
	void HandleNewGameClicked();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Menu")
	void HandleContinueClicked();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Menu")
	void HandleLoadGameClicked();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Menu")
	void HandleOptionsClicked();

	UFUNCTION(BlueprintCallable, Category = "Retrieve|Menu")
	void HandleQuitClicked();

private:
	void ShowTitleCard();
	void ShowMenuPane();
	void FocusNextTick(TWeakObjectPtr<UWidget> Target);

	ARetrievePlayerController* GetRetrievePlayerController() const;

	bool bTitleCardDismissed = false;
};
