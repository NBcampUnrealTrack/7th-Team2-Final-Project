#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "UI/Puzzle/RetrievePuzzleTypes.h"
#include "RetrievePuzzleTableRow.generated.h"

class URetrieveInteractionResultAsset;
class URetrieveLootTableAsset;

/**
 * DataTable 한 행 = 고정 퍼즐 하나.
 * DT_Puzzles 같은 DataTable의 Row Struct로 지정해, 퍼즐이 많아도 에셋 하나로 관리한다.
 *
 * 행 채우기:
 *   - RetrievePuzzleDataAsset(생성기)에서 [Generate From Params]로 보드를 만든 뒤,
 *     그 Board(필요하면 Solution)를 디테일 패널에서 복사 → 이 행에 붙여넣기.
 *   - 또는 Board를 행에서 직접 편집.
 *   (생성 파라미터는 생성기 쪽에만 있으면 되므로 행에는 두지 않는다.)
 */
USTRUCT(BlueprintType)
struct FRetrievePuzzleTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle")
	FText DisplayName;

	// 고정 보드(생성기에서 복사해 붙여넣거나 직접 편집).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle")
	FRetrievePuzzleBoardDef Board;

	// 정답 분할(힌트/디버그용, 옵션).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle")
	FRetrievePuzzleSolution Solution;

	// 풀면 적용할 결과(보상). 기존 상호작용 Result 체계(픽업/CustomEvent/Composite) 재사용.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle")
	TArray<TObjectPtr<URetrieveInteractionResultAsset>> SolveResults;

	// 풀면 굴려서 지급할 루트테이블(확률 드롭). DA_LootTable_*을 바로 넣음. SolveResults와 함께 적용됨.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle")
	TArray<TObjectPtr<URetrieveLootTableAsset>> SolveLootTables;
};
