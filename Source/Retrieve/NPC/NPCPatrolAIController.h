#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "NPCPatrolAIController.generated.h"

class UStateTreeAIComponent;
class UStateTree;

/**
 * 마을 배경 NPC(ARetrieveVillagerCharacter) 전용 최소 AIController.
 * Enemy/Lumen 쪽 전투 AI와 완전히 분리되어 있으며, Perception/Team 없이
 * StateTree 하나(순찰)만 구동합니다.
 */
UCLASS()
class RETRIEVE_API ANPCPatrolAIController : public AAIController
{
	GENERATED_BODY()

public:
	ANPCPatrolAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** 대화 시작 등으로 순찰을 잠시 멈출 때 호출. StateTree 로직과 이동을 정지한다. */
	void Deactivate();

	/** 대화 종료 등으로 순찰을 재개할 때 호출. */
	void Reactivate();

protected:
	virtual void OnPossess(APawn* InPawn) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|AI")
	TObjectPtr<UStateTreeAIComponent> StateTreeAIComp;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|AI")
	TObjectPtr<UStateTree> DefaultStateTree;

private:
	void TryStartStateTree();
};
