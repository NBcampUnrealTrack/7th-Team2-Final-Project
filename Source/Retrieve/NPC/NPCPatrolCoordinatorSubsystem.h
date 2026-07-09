#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "NPCPatrolCoordinatorSubsystem.generated.h"

class AAIController;

/**
 * 순찰 중인 NPC들이 서로 너무 가까운 지점을 동시에 목적지로 잡지 않도록
 * 각 AIController가 현재 향하는 목적지를 등록/조회하는 경량 조정자.
 * 실제 충돌 회피(RVO)는 CharacterMovementComponent가 담당하고, 이 서브시스템은
 * "애초에 같은 자리로 몰리는 것"만 완화한다.
 */
UCLASS()
class RETRIEVE_API UNPCPatrolCoordinatorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** RequestingController를 제외한 다른 예약 목적지 중 MinSeparation보다 가까운 것이 있으면 true. */
	bool IsPointTooCrowded(const FVector& Point, const AAIController* RequestingController, float MinSeparation) const;

	/** Controller가 새로 향하는 목적지를 등록(기존 등록은 대체). */
	void ReserveDestination(const AAIController* Controller, const FVector& Point);

	/** 순찰 종료/정지 시 등록 해제. */
	void ReleaseDestination(const AAIController* Controller);

	/** 순찰 중(이동/대기) NPC로 등록. 다른 NPC와의 "마주침" 판정 대상이 된다. */
	void RegisterActivePatroller(AAIController* Controller);

	/** 대화/정지 등으로 순찰에서 빠질 때 등록 해제. */
	void UnregisterActivePatroller(const AAIController* Controller);

	/** Requester를 제외하고, Location에서 Radius 이내에 있는 다른 순찰 중 NPC 컨트롤러를 찾는다. 없으면 nullptr. */
	AAIController* FindNearbyPatroller(const FVector& Location, float Radius, const AAIController* Requester) const;

private:
	TMap<TWeakObjectPtr<const AAIController>, FVector> ReservedDestinations;
	TArray<TWeakObjectPtr<AAIController>> ActivePatrollers;
};
