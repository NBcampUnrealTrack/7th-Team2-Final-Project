#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WeaponTraceLibrary.generated.h"

class UMeshComponent;

USTRUCT()
struct FWeaponTraceSegment
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FVector> Points;

	float Radius = 0.f;

	bool IsValidTrace() const { return Points.Num() > 0 && Radius > 0.f; }
};

/**
 * 무기 메시 → 공격 히트 트레이스 지오메트리 산출 유틸(상태 없음)
 * - 소켓 모드: 기존 GA_Attack::BuildTracePoints 로직을 추출(두 소켓 사이 보간).
 * - 바운드 모드: 메시 로컬 바운드의 최장축을 '날'로 보고 시작/끝/반경 자동 산출(검+방패 공용).
 * 완전대체 준비: 호출부는 모드만 분기하므로, 나중에 소켓 경로를 제거해도 이 API는 유지된다.
 */
UCLASS()
class RETRIEVE_API URetrieveWeaponTraceLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static bool BuildSocketTrace(const UMeshComponent* Mesh, FName StartSocket, FName EndSocket,
		int32 SegmentCount, float Radius, FWeaponTraceSegment& OutSegment);
	
	// RadiusScale: 단면 절반에 곱할 배율. LengthPadding: 날 축 양 끝 패딩
	static bool BuildBoundsTrace(const UMeshComponent* Mesh, float RadiusScale, float LengthPadding,
		int32 SegmentCount, FWeaponTraceSegment& OutSegment);

	// 바운드 중심에 구체 1개(둥근 방패 등). Radius = 최대 절반 크기(면 반지름) × 배율
	static bool BuildBoundsSphere(const UMeshComponent* Mesh, float RadiusScale, FWeaponTraceSegment& OutSegment);
};
