#pragma once

#include "CoreMinimal.h"
#include "UI/RetrieveGamePanelWidget.h"
#include "UI/Puzzle/RetrievePuzzleBoardWidget.h"
#include "RetrievePuzzlePanelWidget.generated.h"

// 패널 안의 퍼즐이 처음 풀렸을 때 1회 브로드캐스트.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRetrievePuzzlePanelSolvedSignature);

/**
 * 퍼즐 보드를 감싸는 패널. PlayerController.OpenExclusivePanel 흐름으로 열린다(커서/입력모드 위임).
 * WBP에는 'Board' 이름의 RetrievePuzzleBoardWidget을 배치해야 한다(BindWidget).
 */
UCLASS(Abstract, Blueprintable)
class RETRIEVE_API URetrievePuzzlePanelWidget : public URetrieveGamePanelWidget
{
	GENERATED_BODY()

public:
	// 고정 보드를 로드하고 풀림 감지를 연결한다.
	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void SetupPuzzle(const FRetrievePuzzleBoardDef& InBoard);

	// 플레이어가 그린 영역을 전부 초기화(리셋 버튼용).
	UFUNCTION(BlueprintCallable, Category = "Puzzle")
	void ResetBoard();

	UFUNCTION(BlueprintPure, Category = "Puzzle")
	URetrievePuzzleBoardWidget* GetBoard() const { return Board; }

	// 퍼즐이 처음 풀린 순간 발동(액터가 구독해 보상 처리).
	UPROPERTY(BlueprintAssignable, Category = "Puzzle")
	FRetrievePuzzlePanelSolvedSignature OnPuzzleSolved;

	// 퍼즐이 처음 풀린 순간 호출 — WBP에서 배너/애니메이션/사운드 연출을 구현한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Puzzle", meta = (DisplayName = "On Puzzle Solved"))
	void BP_OnPuzzleSolved();

protected:
	virtual void NativeDestruct() override;

	// WBP에 'Board' 이름으로 배치 필수.
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<URetrievePuzzleBoardWidget> Board;

	UFUNCTION()
	void HandleBoardSolvedChanged(bool bIsSolved);

private:
	bool bAlreadySolved = false;
};
