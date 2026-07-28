#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UI/Puzzle/RetrievePuzzleTypes.h"
#include "UI/Puzzle/RetrievePuzzleGenerator.h"
#include "RetrievePuzzleDataAsset.generated.h"

class URetrieveInteractionResultAsset;

/**
 * 고정 퍼즐 한 판의 정의 에셋.
 *
 * 워크플로:
 *   1. Content Browser에서 이 클래스로 DA_Puzzle_XXX 생성
 *   2. GenParams(특히 Seed 고정) 설정 후 디테일 패널의 [Generate From Params] 버튼 클릭
 *      → 생성기가 Board/Solution을 채워 굳힘. 저장하면 그 보드가 영구 고정(항상 같은 퍼즐)
 *      (또는 Board를 직접 편집해도 됨)
 *   3. SolveResults에 풀었을 때 줄 결과(기존 상호작용 Result 에셋)를 연결
 */
UCLASS(BlueprintType)
class RETRIEVE_API URetrievePuzzleDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle")
	FText DisplayName;

	// 저장되는 고정 보드. [Generate From Params]로 굳히거나 직접 편집.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle")
	FRetrievePuzzleBoardDef Board;

	// 정답 분할(힌트/디버그용). [Generate From Params] 시 함께 채워짐.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Puzzle")
	FRetrievePuzzleSolution Solution;

	// 풀면 적용할 결과(보상). 기존 상호작용 Result 체계(픽업/루트/CustomEvent/Composite) 재사용.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Puzzle")
	TArray<TObjectPtr<URetrieveInteractionResultAsset>> SolveResults;

	// 생성 파라미터. Seed를 고정하면 [Generate] 결과가 항상 동일.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Gen")
	FRetrievePuzzleGenParams GenParams;

	// 에디터 전용: GenParams로 Board/Solution을 생성해 이 에셋에 굳힌다.
	UFUNCTION(CallInEditor, Category = "Puzzle|Gen", meta = (DisplayName = "Generate From Params"))
	void GenerateFromParams();
};
