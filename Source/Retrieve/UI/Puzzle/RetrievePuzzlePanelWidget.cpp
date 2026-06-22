#include "UI/Puzzle/RetrievePuzzlePanelWidget.h"

void URetrievePuzzlePanelWidget::SetupPuzzle(const FRetrievePuzzleBoardDef& InBoard)
{
	bAlreadySolved = false;

	if (!Board)
	{
		return;
	}

	Board->OnSolvedChanged.AddUniqueDynamic(this, &URetrievePuzzlePanelWidget::HandleBoardSolvedChanged);
	Board->LoadBoardDefinition(InBoard);
}

void URetrievePuzzlePanelWidget::ResetBoard()
{
	// 다시 풀면 클리어 연출이 재발동되도록 풀림 상태도 초기화.
	bAlreadySolved = false;
	if (Board)
	{
		Board->ClearAllRegions();
	}
}

void URetrievePuzzlePanelWidget::HandleBoardSolvedChanged(bool bIsSolved)
{
	// 처음 풀린 순간 1회만 알림(다시 그려서 풀어도 재발동 안 함).
	if (bIsSolved && !bAlreadySolved)
	{
		bAlreadySolved = true;
		OnPuzzleSolved.Broadcast();   // 외부(액터) — 보상 적용
		BP_OnPuzzleSolved();          // WBP — 화면 연출
	}
}

void URetrievePuzzlePanelWidget::NativeDestruct()
{
	if (Board)
	{
		Board->OnSolvedChanged.RemoveDynamic(this, &URetrievePuzzlePanelWidget::HandleBoardSolvedChanged);
	}
	Super::NativeDestruct();
}
