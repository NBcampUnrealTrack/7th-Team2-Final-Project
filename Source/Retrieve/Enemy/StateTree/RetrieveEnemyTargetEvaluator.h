#pragma once

#include "CoreMinimal.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeExecutionTypes.h"
#include "RetrieveEnemyTargetEvaluator.generated.h"

class AAIController;
class APawn;

USTRUCT(BlueprintType)
struct FRetrieveEnemyTargetEvalInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output")
	TObjectPtr<AActor> TargetPlayer = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output")
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output")
	float DistanceToTarget = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output")
	bool bTargetLost = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output")
	FVector SpawnedLocation = FVector::ZeroVector;
	
	UPROPERTY(EditAnywhere, Category = "Output")
	FGameplayTagContainer OwnedTags;
	
	UPROPERTY(EditAnywhere, Category = "Output")
	bool bOutOfChaseRange = false;
	
	UPROPERTY(EditAnywhere, Category = "Input")
	float ChaseRange = 1500.f;
	
	float AccumulatedTime = 0.f;
	float TimeSinceLastSeen = 0.f;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Enemy Target Evaluator", Category = "Retrieve|AI"))
struct RETRIEVE_API FRetrieveEnemyTargetEvaluator : public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FRetrieveEnemyTargetEvalInstanceData;

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual void TreeStart(FStateTreeExecutionContext& Context) const override;
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	// 타깃을 마지막으로 인지한 후 소실로 판정하기까지의 유예 시간(초)
	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.0"))
	float TargetLostDelay = 3.f;

	// Perception 쿼리 최소 간격 (성능 절감)
	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.05"))
	float TickInterval = 0.2f;

private:
	TStateTreeExternalDataHandle<AAIController> AIControllerHandle;
	TStateTreeExternalDataHandle<APawn> PawnHandle;
};
