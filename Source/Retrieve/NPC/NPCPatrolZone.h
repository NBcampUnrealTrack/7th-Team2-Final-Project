#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NPCPatrolZone.generated.h"

class UBoxComponent;

/**
 * 레벨에 배치하는 NPC 순찰 구역 볼륨.
 * ARetrieveVillagerCharacter::PatrolZone에 연결하면, 순찰 지점 랜덤 선정 시
 * 이 박스 내부에 들어오는 지점만 채택한다(개별 스폰 반경 대신 구역 전체를 공유).
 */
UCLASS()
class RETRIEVE_API ANPCPatrolZone : public AActor
{
	GENERATED_BODY()

public:
	ANPCPatrolZone();

	/** 구역 중심(액터 위치). 순찰 지점 탐색의 기준점으로 쓰인다. */
	FVector GetZoneCenter() const;

	/** 구역을 감싸는 탐색 반경(박스 바운딩 스피어 기준). */
	float GetZoneRadius() const;

	/** 월드 좌표 Point가 이 구역(축 정렬 박스) 내부에 있는지 검사. */
	bool ContainsPoint(const FVector& Point) const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Retrieve|AI|Patrol")
	TObjectPtr<UBoxComponent> ZoneBounds;
};
