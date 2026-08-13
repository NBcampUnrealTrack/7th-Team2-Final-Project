#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GenericTeamAgentInterface.h"
#include "Data/RetrieveDataTableTypes.h"
#include "EnemyAIController.generated.h"

class UStateTreeAIComponent;
class UAIPerceptionComponent;
class UAISenseConfig_Damage;
class UAISenseConfig_Sight;
class UStateTree;

UCLASS()
class RETRIEVE_API AEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	AEnemyAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	
	void ConfigureStateTree(UStateTree* InStateTree);
	
	virtual ETeamAttitude::Type GetTeamAttitudeTowards(const AActor& Other) const override;
	
	void Deactivate();

	void Reactivate();

	float GetEffectiveSightRadius() const;
	float GetEffectiveLoseSightRadius() const;
	
	/** Enemy Target Evaluator가 현재 추적 중인 대상을 다른 AI 시스템에 공유한다. */
	void SetRecognizedTarget(AActor* InTarget);
	bool IsRecognizingTarget(const AActor* Target) const;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

	virtual void PostInitializeComponents() override;

private:
	void InitSightConfig();
	void InitDamageConfig();
	float GetEffectivePeripheralVisionAngleDegrees() const;
	
	void RestartStateTree();
	void TryStartStateTree();

	TWeakObjectPtr<AActor> RecognizedTarget;
	
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|AI")
	TObjectPtr<UStateTreeAIComponent> StateTreeAIComp;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|AI")
	TObjectPtr<UStateTree> DefaultStateTree;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|AI")
	TObjectPtr<UAIPerceptionComponent> AIPerceptionComp;
	
	// ---- Perception ----
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|AI")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|AI")
	TObjectPtr<UAISenseConfig_Damage> DamageConfig;
	
	/** 유효한 MonsterData 행이 없을 때 사용하는 감지 반경 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|AI|Perception")
	float SightRadius = 1500.f;
	
	/** 유효한 MonsterData 행이 없을 때 사용하는 소실 반경 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|AI|Perception")
	float LoseSightRadius = 1800.f;
	
	/** 수평 시야의 반각. 캐릭터 오버라이드가 이 값보다 크면 시야각을 확장한다. 180이면 전방위다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|AI|Perception")
	float PeripheralVisionAngleDegrees = 180.f;

	// ---- Team ----
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Retrieve|AI|Team")
	ERetrieveTeam Team = ERetrieveTeam::Enemy;
};
