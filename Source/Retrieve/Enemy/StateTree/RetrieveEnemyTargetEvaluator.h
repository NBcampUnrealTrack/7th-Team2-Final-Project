#pragma once

#include "CoreMinimal.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeExecutionTypes.h"
#include "RetrieveEnemyTargetEvaluator.generated.h"

class AAIController;
class APawn;
class UEnemyCombatComponent;
class UEnemySuspicionIndicatorComponent;

USTRUCT(BlueprintType)
struct FRetrieveEnemyTargetEvalInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output")
	TObjectPtr<AActor> TargetPlayer = nullptr;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output")
	FVector TargetLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output")
	float AttackableRange = 200.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output")
	float StrafeOffRange = 450.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output")
	float StrafeMinNoise = -100.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output")
	float StrafeMaxNoise = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output")
	float OrbitInnerRadius = 160.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output")
	float OrbitOuterRadius = 300.f;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output")
	float DistanceToTarget = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output")
	bool bTargetLost = true;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output")
	FVector SpawnedLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Output")
	FGameplayTagContainer OwnedTags;
	
	UPROPERTY(EditAnywhere, Category = "Output")
	bool bOutOfChaseRange = false;
	
	bool bWasOutOfChaseRange = false;
	
	UPROPERTY(EditAnywhere, Category = "Output")
	float ChaseRange = 1500.f;
	
	UPROPERTY(EditAnywhere, Category = "Output")
	float RechasableRange = 100.f;
	
	UPROPERTY(EditAnywhere, Category = "Output")
	float MoveAcceptableRadius = 5.f;

	UPROPERTY(EditAnywhere, Category = "Output")
	bool bUseDirectChaseToTarget = false;

	UPROPERTY(EditAnywhere, Category = "Output")
	bool bPatrolable = false;

	UPROPERTY(EditAnywhere, Category = "Output")
	bool bHasAerialPhase = false;

	UPROPERTY(EditAnywhere, Category = "Output")
	float PatrolRange = 1200.0f;
	
	UPROPERTY(EditAnywhere, Category = "Output")
	bool bAttackable = false;

	// Attack 상태 진입 전용 — MaxActivationRange 기준(AttackableRange보다 넓음).
	// Attack Task가 진입 후 실제 사거리까지 접근할 수 있도록 별도로 둔다.
	UPROPERTY(EditAnywhere, Category = "Output")
	bool bAttackApproachable = false;

	UPROPERTY(EditAnywhere, Category = "Output")
	bool bSpecialAttackable = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output")
	FVector ChaseLocation = FVector::ZeroVector;
	
	UPROPERTY(EditAnywhere, Category = "Output")
	bool bHasToken = false;

	// 경계(Suspicious) 게이지, 0~1 정규화. UI에서 그대로 구독 가능.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Output")
	float SuspicionGauge = 0.f;

	UPROPERTY(EditAnywhere, Category = "Output")
	bool bSuspicionGaugeFull = false;

	bool bWasSuspicionGaugeFull = false;

	float SuspicionIncreaseRate = 0.f;
	float SuspicionDecreaseRate = 0.f;
	float ForceCombatRange = 0.f;   
	
	float AccumulatedTime = 0.f;
	float TimeSinceLastSeen = 0.f;
	
	UPROPERTY()
	TWeakObjectPtr<UEnemyCombatComponent> CachedCombatComponent = nullptr;

	UPROPERTY()
	TWeakObjectPtr<UEnemySuspicionIndicatorComponent> CachedSuspicionIndicator = nullptr;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Enemy Target Evaluator", Category = "Retrieve|AI"))
struct RETRIEVE_API FRetrieveEnemyTargetEvaluator : public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FRetrieveEnemyTargetEvalInstanceData;

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual void TreeStart(FStateTreeExecutionContext& Context) const override;
	virtual void TreeStop(FStateTreeExecutionContext& Context) const override;
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	// 타깃을 마지막으로 인지한 후 소실로 판정하기까지의 유예 시간(초)
	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.0"))
	float TargetLostDelay = 3.f;

	// Perception 쿼리 최소 간격 (성능 절감)
	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.05"))
	float TickInterval = 0.2f;
	
	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float HorizontalHalfFOV = 60.f;

	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.0"))
	float AggroCrowdWeight = 0.35f; // 0 = 순수하게 가장 가까운 대상

	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float TargetSwitchHysteresis = 0.8f; // 명확하게 더 나은 대상이 아니면 현재 타겟을 유지

private:
	TStateTreeExternalDataHandle<AAIController> AIControllerHandle;
	TStateTreeExternalDataHandle<APawn> PawnHandle;
};
