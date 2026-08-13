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
	TObjectPtr<AActor> TargetPlayer = nullptr;
	
	UPROPERTY(EditAnywhere, Category = "Input", meta=(Optional))
	float AttackRange = 200.f;
	
	UPROPERTY(EditAnywhere, Category = "Input", meta=(Optional))
	float DistanceToTarget = TNumericLimits<float>::Max();
	
	UPROPERTY(EditAnywhere, Category = "Input")
	FVector ChaseLocation = FVector::ZeroVector;
	
	UPROPERTY(EditAnywhere, Category = "Input", meta = (ClampMin = "0.0"))
	float MoveAcceptableRadius = 30.f;
	
	UPROPERTY(EditAnywhere, Category="Input", meta=(Optional))
	FName SelectedPatternRowName = NAME_None;
	
	// 공격 요청 후 이 시간(초)을 초과하면 GA 완료 여부와 무관하게 Failed 반환
	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.5"))
	float MaxAttackDuration = 5.f;

	// 공격 요청 전 접근 단계가 이 시간을 초과하면 Failed를 반환한다.
	// 0이면 접근 타임아웃을 사용하지 않는다.
	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.0"))
	float MaxApproachDuration = 5.f;

	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.0"))
	float AttackStartRangeTolerance = 20.f;

	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.0"))
	float AttackStartDelay = 0.4f;

	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.0"))
	float AttackStartGraceTime = 1.5f;

	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float FacingAcceptanceAngle = 8.f;

	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.0"))
	float FacingInterpSpeed = 8.f;

	float ElapsedTime = 0.f;
	float TimeInSoftAttackRange = 0.f;
	float TimeSinceAttackRequested = 0.f;
	float ApproachElapsedTime = 0.f;
	
	bool bStartAttack = false;
	bool bObservedPatternActive = false;
	bool bAttackTokenAcquired = false;
	bool bInAttackWindow = false;
	
	FVector LastMoveRequestLocation = FVector::ZeroVector;
	
	UPROPERTY()
	TWeakObjectPtr<UEnemyCombatComponent> CachedCombatComponent = nullptr;

	// Attack 진입 전 RVO 회피 설정. 접근 중 플레이어를 우회하지 않도록 Attack 동안만 끈다.
	bool bOriginalUseRVOAvoidance = true;

	// 접근 중 FaceTargetForAttack만 회전을 제어하도록 기존 회전 설정을 저장하고 일시적으로 비활성화한다.
	// 전진 로코모션 Enemy는 기존 설정을 변경하지 않는다.
	bool bOriginalOrientRotationToMovement = true;
	bool bOriginalUseControllerDesiredRotation = false;
	bool bOriginalUseControllerRotationYaw = false;
	bool bRotationSettingsCaptured = false;
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
