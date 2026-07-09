#pragma once

#include "CoreMinimal.h"
#include "Character/RetrieveNPCCharacter.h"
#include "RetrieveVillagerCharacter.generated.h"

class UAnimMontage;

/**
 * 마을 배경 NPC 베이스 클래스. 상점 NPC(ARetrieveNPCCharacter, 고정)와 달리
 * ANPCPatrolAIController + StateTree를 통해 스폰 지점 주변을 무작위로 순찰합니다.
 */
UCLASS()
class RETRIEVE_API ARetrieveVillagerCharacter : public ARetrieveNPCCharacter
{
	GENERATED_BODY()

public:
	ARetrieveVillagerCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	FVector GetInitialSpawnLocation() const { return InitialSpawnLocation; }

	/** 순찰 기준점(스폰 위치) 중심 반경. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Patrol", meta = (ClampMin = "50.0"))
	float PatrolRadius = 800.f;

	/** 순찰 지점 도착 후 다음 지점으로 출발하기까지의 최소 대기 시간(초). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Patrol", meta = (ClampMin = "0.0"))
	float PatrolWaitTimeMin = 2.f;

	/** 순찰 지점 도착 후 다음 지점으로 출발하기까지의 최대 대기 시간(초). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Patrol", meta = (ClampMin = "0.0"))
	float PatrolWaitTimeMax = 5.f;

	/** 순찰 지점 도착으로 판정하는 허용 반경(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Patrol", meta = (ClampMin = "10.0"))
	float PatrolAcceptanceRadius = 120.f;

	/** 이동이 이 시간(초)을 넘기면 막힌 것으로 보고 다른 지점을 재선정합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Patrol", meta = (ClampMin = "1.0"))
	float PatrolMaxMoveTime = 8.f;

	/**
	 * 순찰 구역 볼륨(선택). 레벨에 배치한 ANPCPatrolZone을 지정하면 스폰 위치 기준 반경 대신
	 * 이 구역 내부로 순찰 지점을 제한합니다. 미지정 시 기존 PatrolRadius 방식을 그대로 사용합니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Patrol")
	TObjectPtr<class ANPCPatrolZone> PatrolZone;

	/** 다른 NPC가 이미 향하고 있는 지점과 이 거리(cm) 이상 떨어진 곳만 다음 순찰 지점으로 채택합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Patrol", meta = (ClampMin = "0.0"))
	float PatrolMinNeighborSeparation = 250.f;

	/**
	 * 순찰 지점 도착 후 대기하는 동안 재생할 수 있는 "일상 행동" 몽타주 목록(앉기/줍기 등).
	 * 비워두면 기존처럼 그냥 서서 대기합니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Patrol|Idle")
	TArray<TObjectPtr<UAnimMontage>> IdleActionMontages;

	/** 대기 시작 시 위 목록 중 하나를 재생할 확률(0~1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Patrol|Idle", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IdleActionChance = 0.35f;

	/**
	 * 다른 순찰 NPC와 마주쳤을 때 재생할 "대화" 제스처 몽타주(선택).
	 * 비워두면 몽타주 없이 멈춰서 서로 바라보기만 합니다.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Patrol|Greet")
	TObjectPtr<UAnimMontage> GreetMontage;

	/** 이 거리(cm) 안으로 다른 순찰 NPC가 들어오면 대화 행동을 트리거합니다. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Patrol|Greet", meta = (ClampMin = "0.0"))
	float GreetTriggerRadius = 200.f;

	/** 대화 행동 지속 시간(초, 몽타주가 더 길면 몽타주 길이를 따름). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Patrol|Greet", meta = (ClampMin = "0.5"))
	float GreetDuration = 3.f;

	/** 같은 NPC가 다시 대화를 트리거하기까지의 최소 쿨다운(초). 계속 붙어있을 때 반복 트리거 방지. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Retrieve|Patrol|Greet", meta = (ClampMin = "0.0"))
	float GreetCooldown = 15.f;

	/**
	 * Synty 인사/일상행동 애니메이션이 IK 리타겟 파이프라인으로 구축된 팩의 메시만 골라 쓰도록 만든
	 * 드롭다운. 목록은 실제로 리타겟 파이프라인이 있는 폴더에서 에셋 레지스트리로 스캔해 채운다.
	 * 값을 고르면 즉시 GetMesh()의 스켈레탈 메시를 교체한다.
	 */
	UPROPERTY(EditAnywhere, Category = "Retrieve|Villager|Mesh", meta = (GetOptions = "GetVillagerMeshOptions"))
	FString VillagerMeshOption;

	UFUNCTION()
	TArray<FString> GetVillagerMeshOptions() const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual void BeginPlay() override;

private:
	FVector InitialSpawnLocation = FVector::ZeroVector;
};
