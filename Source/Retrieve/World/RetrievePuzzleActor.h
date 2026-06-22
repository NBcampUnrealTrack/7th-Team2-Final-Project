#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "RetrievePuzzleActor.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class URetrieveInteractionResponseComponent;
class URetrievePuzzlePanelWidget;

// 퍼즐을 처음 풀었을 때 발동(보상 적용 후).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRetrievePuzzleActorSolvedSignature, AActor*, InteractionInstigator);

/**
 * 월드에 배치하는 퍼즐 상호작용 액터 (Bonfire 패턴).
 *
 * 흐름: 상호작용 → InteractionComponent.OnApplied → 퍼즐 패널 오픈(고정 DataAsset 로드)
 *       → 드래그로 풀기 → 패널 OnPuzzleSolved → SolveResults 적용 + OnPuzzleSolved 발동.
 *
 * 상호작용 트리거:
 *   - BP에서 Manager_InteractionTarget(상용 플러그인) 컴포넌트를 붙이면 ResponseComponent가
 *     자동 바인딩되어 OnApplied가 발동된다(Bonfire와 동일).
 *   - 또는 테스트/단순 트리거용으로 BP/레벨에서 OpenPuzzleFor()를 직접 호출해도 된다.
 *
 * 1회성 보상: 처음 풀 때만 SolveResults가 적용된다. 이후 다시 열면 보드는 재생성되어
 *            다시 풀 수 있지만 보상은 재지급되지 않는다.
 *
 * 네트워킹: 로컬/싱글 우선. MP에서는 SolveResults 적용을 서버 권위로 위임하도록 확장 필요.
 */
UCLASS(Blueprintable)
class RETRIEVE_API ARetrievePuzzleActor : public AActor
{
	GENERATED_BODY()

public:
	ARetrievePuzzleActor();

	// 이 액터가 여는 고정 퍼즐 (DataTable + 행 선택). Row Struct는 FRetrievePuzzleTableRow.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Puzzle",
		meta = (RowType = "/Script/Retrieve.RetrievePuzzleTableRow"))
	FDataTableRowHandle PuzzleRow;

	// 열 퍼즐 패널 위젯(WBP). RetrievePuzzlePanelWidget 자식.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Puzzle")
	TSoftClassPtr<URetrievePuzzlePanelWidget> PuzzlePanelClass;

	// 패널을 닫는 토글 키(베이스 패널이 처리). 비우면 WBP의 닫기 버튼/ESC 등에 의존.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Retrieve|Puzzle")
	FKey PanelToggleKey;

	// 상호작용 완료 후 재상호작용(다시 열기)을 위한 외부 InteractionManager FinishMethod enum 값.
	UPROPERTY(EditDefaultsOnly, Category = "Retrieve|Puzzle|Interaction")
	uint8 PersistentFinishMethodValue = 3;

	UPROPERTY(BlueprintReadOnly, Category = "Retrieve|Puzzle")
	bool bSolved = false;

	UPROPERTY(BlueprintAssignable, Category = "Retrieve|Puzzle")
	FRetrievePuzzleActorSolvedSignature OnPuzzleSolved;

	// 퍼즐 패널을 연다(상호작용 핸들러이자, 테스트용 직접 호출 진입점).
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Puzzle")
	void OpenPuzzleFor(AActor* InteractionInstigator);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<UStaticMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<URetrieveInteractionResponseComponent> InteractionComponent;

	// 외부 InteractionManager 감지 컴포넌트(플러그인 BP). 타입 의존을 피하려 UActorComponent로 보관.
	// 생성자에서 "InteractionTarget" 이름으로 생성 → InteractionComponent가 BeginPlay에서 자동 바인딩.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|Components")
	TObjectPtr<UActorComponent> InteractionTargetComponent;

	// InteractionComponent.OnApplied 바인딩 핸들러.
	UFUNCTION()
	void HandlePuzzleInteracted(AActor* InteractionInstigator);

	// 패널 OnPuzzleSolved 바인딩 핸들러.
	UFUNCTION()
	void HandlePuzzleSolved();

private:
	URetrievePuzzlePanelWidget* OpenPuzzlePanel(AActor* InteractionInstigator);
	void ApplySolveResults(AActor* InteractionInstigator);

	// 외부 InteractionManager 타겟을 재상호작용 가능하도록 설정(리플렉션).
	void ConfigurePersistentInteractionTarget() const;

	TWeakObjectPtr<AActor> PendingInstigator;
	TWeakObjectPtr<URetrievePuzzlePanelWidget> ActivePuzzlePanel;
};
