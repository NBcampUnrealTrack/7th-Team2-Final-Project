#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "StateTreeExecutionTypes.h"
#include "StateTreeTask_EnemyPatternAttack.generated.h"

class APawn;
class UEnemyCombatComponent;

USTRUCT(BlueprintType)
struct FStateTreeTask_EnemyPatternAttackInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, meta=(Optional))
	TObjectPtr<AActor> TargetPlayer = nullptr;
	
	UPROPERTY(EditAnywhere, Category = "Input", meta=(Optional))
	float DistanceToTarget = TNumericLimits<float>::Max();

	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.5"))
	float MaxAttackDuration = 8.f;

	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.0"))
	float AttackStartGraceTime = 0.6f;

	float ElapsedTime = 0.f;
	float TimeSinceAttackRequested = 0.f;

	bool bStartAttack = false;
	bool bObservedPatternActive = false;

	UPROPERTY()
	TWeakObjectPtr<UEnemyCombatComponent> CachedCombatComponent = nullptr;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Enemy Pattern Attack", Category = "Retrieve|AI"))
struct RETRIEVE_API FStateTreeTask_EnemyPatternAttack : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTask_EnemyPatternAttackInstanceData;

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

private:
	TStateTreeExternalDataHandle<APawn> PawnHandle;
};
