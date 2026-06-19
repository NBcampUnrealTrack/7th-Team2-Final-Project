#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Data/RetrieveDataTableTypes.h"
#include "LumenAIController.generated.h"

class UStateTreeAIComponent;

/**
 * Lumen의 AI 컨트롤러, 호스트 전용
 *  - 팀 = Player (적이 EQS 위협 수집 시 Hostile로 인식되도록)
 *  - AIPerception 없음. 전투 후퇴 감지는 팀/반경 EQS 컨텍스트를 사용합니다.
 *  - UStateTreeAIComponent를 통해 ST_Lumen을 구동 (트리 에셋은 BP_LumenAIController에서 할당)
 */
UCLASS()
class RETRIEVE_API ALumenAIController : public AAIController
{
	GENERATED_BODY()

public:
	ALumenAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;

protected:
	virtual void OnPossess(APawn* InPawn) override;

private:
	void TryStartStateTree();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|AI")
	TObjectPtr<UStateTreeAIComponent> StateTreeAIComp;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|AI|Team")
	ERetrieveTeam Team = ERetrieveTeam::Player;
};
