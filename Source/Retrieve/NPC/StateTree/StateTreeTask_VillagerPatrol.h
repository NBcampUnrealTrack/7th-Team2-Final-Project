#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "StateTreeExecutionTypes.h"
#include "StateTreeTask_VillagerPatrol.generated.h"

class APawn;
class ARetrieveVillagerCharacter;

USTRUCT(BlueprintType)
struct FStateTreeTask_VillagerPatrolInstanceData
{
	GENERATED_BODY()

	FVector PatrolOrigin = FVector::ZeroVector;
	FVector CurrentPatrolPoint = FVector::ZeroVector;
	float WaitTimer = 0.f;
	float MoveElapsed = 0.f;
	bool bWaiting = false;
	bool bHasPoint = false;
	bool bOriginCaptured = false;

	/** 다른 순찰 NPC와 마주쳐 "대화" 중인지 여부. */
	bool bGreeting = false;
	float GreetTimer = 0.f;
	/** 마지막 대화 종료 후 다음 대화 트리거까지 남은 쿨다운(초). */
	float GreetCooldownRemaining = 0.f;
};

/**
 * 마을 NPC(ARetrieveVillagerCharacter) 전용 순찰 태스크.
 * 스폰 지점 주변 반경 내 도달 가능한 임의 지점으로 이동 → 대기 → 재선정을 무한 반복합니다.
 * 별도 Evaluator 없이 Pawn을 ARetrieveVillagerCharacter로 캐스팅해 파라미터를 직접 읽습니다.
 */
USTRUCT(BlueprintType, meta = (DisplayName = "Villager Patrol", Category = "Retrieve|AI|NPC"))
struct RETRIEVE_API FStateTreeTask_VillagerPatrol : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FStateTreeTask_VillagerPatrolInstanceData;

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

private:
	bool PickAndMoveToNewPoint(FInstanceDataType& InstanceData, APawn* Pawn) const;

	/** 대기 시작 시 확률적으로 "일상 행동" 몽타주를 재생하고, 필요하면 WaitTimer를 몽타주 길이에 맞춰 늘린다. */
	void TryPlayIdleAction(const ARetrieveVillagerCharacter* Villager, APawn* Pawn, FInstanceDataType& InstanceData) const;

	TStateTreeExternalDataHandle<APawn> PawnHandle;
};
