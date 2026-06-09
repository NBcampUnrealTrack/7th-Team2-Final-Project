#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RetrieveTargetingLibrary.generated.h"

class ACharacter;

/**
 * 오토타게팅/모션워핑 보조 유틸. 상태 없음(stateless) — 공격 시작 시 1회 호출.
 * 적/아군 판정은 엔진 IGenericTeamAgentInterface(FGenericTeamId::GetAttitude)로만.
 * 콘솔변수 "Retrieve.Warp.Debug 1"이면 콘/선택 타겟을 잠깐 그린다.
 */
UCLASS()
class RETRIEVE_API URetrieveTargetingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Source 기준 Range/HalfAngle 콘 안에서 적대 대상 중 가중점수 최고 타겟 반환. 없으면 nullptr.
	// AimDirection: 콘 중심(월드, 내부 2D 정규화). RangeWeightRate: 0~1 거리/각도 블렌드.
	// MaxVerticalDelta: Source와의 수직(Z) 차가 이 값을 넘는 대상은 제외(비평지 닿을 수 없는 타겟 차단).
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Targeting")
	static AActor* FindBestTarget(ACharacter* Source, float Range, float HalfAngle,
		FVector AimDirection, float MaxVerticalDelta, float RangeWeightRate = 0.5f);

	// 워프 트랜스폼 계산(타겟 유효 케이스 전용).
	// 캡슐R 합 + StandoffOffset 만큼 떨어진 지점 + 타겟 정조준. Z는 Source 기준 유지.
	// MaxWarpDistance: 이동량 상한(항상 적용). 보통 그 섹션의 루트모션 전진량을 넘긴다. 0이면 전진 없음(회전만).
	// 타겟이 무효이면 호출부에서 RemoveWarpTarget으로 처리할 것(루트모션 유지).
	UFUNCTION(BlueprintCallable, Category = "Retrieve|Targeting")
	static FTransform BuildWarpTransform(ACharacter* Source, AActor* Target, float StandoffOffset, float MaxWarpDistance);
};
