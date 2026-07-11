#pragma once

#include "CoreMinimal.h"
#include "UI/RetrieveGamePanelWidget.h"
#include "RetrieveSystemMenuWidget.generated.h"

class UButton;

/**
 * 시스템 메뉴(WBP_SystemMenu)의 C++ 베이스.
 * 버튼 OnClicked 배선을 C++에서 처리하므로, WBP는 아래 이름의 버튼만 배치하면 된다(그래프 배선 불필요).
 *   ResumeButton / SettingsButton / CommandsButton / RespawnButton / MainMenuButton / QuitButton  (모두 BindWidgetOptional)
 * ESC·Resume은 RequestClose로 닫힌다. 멀티플레이를 추후 지원할 예정이라 게임을 실제로 일시정지하지는 않는다.
 */
UCLASS(Abstract, Blueprintable)
class RETRIEVE_API URetrieveSystemMenuWidget : public URetrieveGamePanelWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;

	/** "계속하기" — 패널을 닫아 게임으로 복귀(닫힐 때 언포즈). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ResumeButton;

	/** "설정" — 설정 화면을 연다(현재 시스템 메뉴 패널은 교체됨). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SettingsButton;

	/** "조작키 안내" — 조작키 안내 화면을 연다(현재 시스템 메뉴 패널은 교체됨). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CommandsButton;

	/** "리스폰" — 마지막 체크포인트로 강제 리스폰(언스턱). 맵 끼임/낙사 루프 탈출용. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> RespawnButton;

	/** "메인 메뉴로" — SessionState를 MainMenu로 전환. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> MainMenuButton;

	/** "게임 종료" — 애플리케이션 종료. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> QuitButton;

	UFUNCTION()
	void HandleResumeClicked();

	UFUNCTION()
	void HandleSettingsClicked();

	UFUNCTION()
	void HandleCommandsClicked();

	UFUNCTION()
	void HandleRespawnClicked();

	UFUNCTION()
	void HandleMainMenuClicked();

	UFUNCTION()
	void HandleQuitClicked();
};
