#include "Data/Puzzle/RetrievePuzzleDataAsset.h"

DEFINE_LOG_CATEGORY_STATIC(LogRetrievePuzzle, Log, All);

void URetrievePuzzleDataAsset::GenerateFromParams()
{
	FRetrievePuzzleBoardDef NewBoard;
	FRetrievePuzzleSolution NewSolution;
	if (URetrievePuzzleGeneratorLibrary::GeneratePuzzle(GenParams, NewBoard, NewSolution))
	{
		Modify();
		Board = NewBoard;
		Solution = NewSolution;
		MarkPackageDirty();
		UE_LOG(LogRetrievePuzzle, Log, TEXT("[Puzzle] '%s' 생성 완료 (%dx%d, 영역 %d, Seed %d)"),
			*GetName(), Board.GridWidth, Board.GridHeight,
			(Solution.RegionIdPerCell.Num() > 0 ? GenParams.RegionCount : 0), GenParams.Seed);
	}
	else
	{
		UE_LOG(LogRetrievePuzzle, Warning, TEXT("[Puzzle] '%s' 생성 실패 — GenParams 확인(크기/영역수/유니크 모드)."), *GetName());
	}
}
