#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameplayTagContainer.h"
#include "UI/Puzzle/RetrievePuzzleTypes.h"
#include "RetrievePuzzleGenerator.generated.h"

UENUM(BlueprintType)
enum class ERetrievePuzzleUniqueness : uint8
{
	// 풀 수 있는 것만 보장(솔버 미사용, 가장 빠름). 여러 해 가능.
	SolvableOnly UMETA(DisplayName = "Solvable Only"),
	// 제한시간 내 유니크 해를 찾으면 사용, 못 찾으면 마지막 후보(풀 수 있음)로 폴백.
	BestEffort   UMETA(DisplayName = "Best Effort Unique"),
	// 유니크 해를 못 찾으면 실패(false) 반환.
	StrictUnique UMETA(DisplayName = "Strict Unique"),
};

/**
 * 퍼즐 생성 파라미터.
 */
USTRUCT(BlueprintType)
struct FRetrievePuzzleGenParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Gen", meta = (ClampMin = "2"))
	int32 GridWidth = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Gen", meta = (ClampMin = "2"))
	int32 GridHeight = 5;

	// 영역 수(= 각 원소의 개수). 0 이하면 보드 크기로 자동 추정.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Gen")
	int32 RegionCount = 4;

	// 사용할 원소 종류. 비우면 Element.Fire/Water/Wind 3종.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Gen")
	TArray<FGameplayTag> ElementTypes;

	// -1이면 매번 랜덤, 그 외엔 재현 가능한 시드.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Gen")
	int32 Seed = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Gen")
	ERetrievePuzzleUniqueness Uniqueness = ERetrievePuzzleUniqueness::BestEffort;

	// 일부 칸을 비워 불규칙 보드로.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Gen")
	bool bAllowHoles = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Gen", meta = (ClampMin = "0", EditCondition = "bAllowHoles"))
	int32 HoleCount = 0;

	// 유효/유니크 보드를 찾기 위한 재시도 횟수.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Gen", meta = (ClampMin = "1"))
	int32 MaxAttempts = 40;

	// 솔버 1회 탐색 노드 예산(폭주 방지). 초과 시 유니크 미확정 처리.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Puzzle|Gen", meta = (ClampMin = "1000"))
	int32 SolverNodeBudget = 200000;
};

/**
 * 영역 분할 퍼즐 생성/검증 라이브러리.
 *
 * 생성: 해(解) 먼저 방식 — 격자를 N개 연결 영역으로 덮고, 각 영역에 원소를 하나씩 배치.
 *       만든 분할 자체가 한 개의 해이므로 항상 풀 수 있음.
 * 검증: 백트래킹 솔버로 유효한 분할 해의 개수를 셈(유니크 판정/디버그).
 */
UCLASS()
class RETRIEVE_API URetrievePuzzleGeneratorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 파라미터대로 풀 수 있는 보드를 생성. 성공 시 true + OutBoard/OutSolution 채움.
	UFUNCTION(BlueprintCallable, Category = "Puzzle|Gen")
	static bool GeneratePuzzle(const FRetrievePuzzleGenParams& Params, FRetrievePuzzleBoardDef& OutBoard, FRetrievePuzzleSolution& OutSolution);

	// 보드의 유효한 분할 해 개수를 SolutionLimit까지 셈. 노드예산 초과로 미확정이면 -1.
	UFUNCTION(BlueprintCallable, Category = "Puzzle|Gen")
	static int32 CountSolutions(const FRetrievePuzzleBoardDef& Board, int32 SolutionLimit = 2, int32 NodeBudget = 200000);
};
