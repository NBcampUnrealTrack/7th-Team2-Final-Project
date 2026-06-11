#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "StateTreeExecutionTypes.h"
#include "StateTreeTask_EnemyAttack.generated.h"

class APawn;
class UEnemyCombatComponent;

USTRUCT(BlueprintType)
struct FStateTreeTask_EnemyAttackInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta=(Optional))
	TObjectPtr<AActor> TargetActor = nullptr;
	
	UPROPERTY(EditAnywhere, Category = "Input", meta=(Optional))
	float AttackRange = 200.f;
	
	UPROPERTY(EditAnywhere, Category = "Input", meta=(Optional))
	float DistanceToTarget = TNumericLimits<float>::Max();
	
	UPROPERTY(EditAnywhere, Category = "Input")
	FVector ChaseLocation = FVector::ZeroVector;
	
	UPROPERTY(EditAnywhere, Category = "Input", meta = (ClampMin = "0.0"))
	float MoveAcceptableRadius = 30.f;
	
	// 이 시간(초)을 초과하면 GA 완료 여부와 무관하게 Succeeded 반환
	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.5"))
	float MaxAttackDuration = 5.f;

	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.0"))
	float AttackStartRangeTolerance = 20.f;

	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.0"))
	float AttackStartDelay = 0.4f;

	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.0"))
	float AttackStartGraceTime = 1.5f;

	float ElapsedTime = 0.f;
	float TimeInSoftAttackRange = 0.f;
	float TimeSinceAttackRequested = 0.f;
	
	bool bStartAttack = false;
	bool bObservedPatternActive = false;
	
	FVector LastMoveRequestLocation = FVector::ZeroVector;
	
	UPROPERTY()
	TWeakObjectPtr<UEnemyCombatComponent> CachedCombatComponent = nullptr;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Enemy Attack", Category = "Retrieve|AI"))
struct RETRIEVE_API FStateTreeTask_EnemyAttack : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTask_EnemyAttackInstanceData;

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

private:
	TStateTreeExternalDataHandle<APawn> PawnHandle;
};
