#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RetrievePuzzleTypes.generated.h"

/**
 * 영역 분할 퍼즐의 한 칸 상태.
 * 보드 = 격자(직사각형 또는 불규칙). 플레이어가 드래그로 칸을 영역에 묶어
 * "각 영역이 등장하는 모든 원소를 정확히 하나씩" 포함하도록 만들면 클리어.
 */
USTRUCT(BlueprintType)
struct FRetrievePuzzleCell
{
	GENERATED_BODY()

	// 이 칸에 놓인 원소 (없으면 빈 태그). Element.Fire / Element.Water / Element.Wind 사용.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
	FGameplayTag ElementTag;

	// 소속 영역 ID. INDEX_NONE(-1)이면 아직 어느 영역에도 속하지 않음.
	UPROPERTY(BlueprintReadOnly, Category = "Puzzle")
	int32 RegionId = INDEX_NONE;

	// false면 보드에 존재하지 않는 격자(불규칙 보드 형태용 구멍).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
	bool bExists = true;
};

/**
 * 에디터/인라인에서 보드의 원소 배치를 입력하기 위한 항목.
 * (X = Column, Y = Row), 좌상단이 (0,0).
 */
USTRUCT(BlueprintType)
struct FRetrievePuzzleElementPlacement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
	FIntPoint Coord = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
	FGameplayTag ElementTag;
};

/**
 * 한 퍼즐 보드의 완전한 정의. 생성기 결과/저장 포맷/DataAsset 전환의 공용 단위.
 */
USTRUCT(BlueprintType)
struct FRetrievePuzzleBoardDef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
	int32 GridWidth = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
	int32 GridHeight = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
	TArray<FIntPoint> AbsentCells;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle")
	TArray<FRetrievePuzzleElementPlacement> ElementPlacements;
};

/**
 * 한 가지 정답 분할. 칸별 RegionId(row-major: Row*GridWidth+Col).
 * 구멍/미할당 칸은 INDEX_NONE. 힌트/정답표시/디버그에 사용.
 */
USTRUCT(BlueprintType)
struct FRetrievePuzzleSolution
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Puzzle")
	int32 GridWidth = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Puzzle")
	int32 GridHeight = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Puzzle")
	TArray<int32> RegionIdPerCell;
};
