
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "LockOnConfig.generated.h"

/**
 * LockOnComponent의 탐색/스코어링/스위치/해제/디버그 파라미터를 묶은 DataAsset
 * DA_LockOnConfig_Default 인스턴스를 만들어 ReactionComponent에 지정 -> LockOnComp에 주입
 */
UCLASS(BlueprintType)
class RETRIEVE_API ULockOnConfig : public UDataAsset
{
	GENERATED_BODY()
	
public:
	// Search
	// 카메라 기준 트레이스 최대 거리
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Search")
	float MaxDistance = 2500.f;
	// 대상 탐색 SphereTrace 반경
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Search")
	float SphereRadius = 600.f;
	// SphereTrace 대상 ObjectType (DA 에셋에서 Pawn 추가 필요)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Search")
	TArray<TEnumAsByte<EObjectTypeQuery>> SearchObjectTypes;
	// 시야 검증 채널 (기본 Visibility)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Search")
	TEnumAsByte<ECollisionChannel> LineOfSightChannel = ECC_Visibility;
	
	// Scoring
	// 화면 중앙에 가까운 타겟을 얼마나 우선할지 (조준 정확도 중시)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scoring", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ScreenWeight = 0.7f;
	// 플레이어(카메라)에 가까운 타겟을 얼마나 우선할지 (근거리 적 중시)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scoring", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DistanceWeight = 0.3f;
	// 화면 중앙으로부터 정규화 거리 이 이상이면 후보 제외
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Scoring", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxScreenDistance = 0.8f;
	
	// Break
	// 타겟과의 거리가 이 값을 넘으면 자동 해제
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Break")
	float BreakDistance = 3000.f;
	// 자동 해제 체크 주기(초). 0.1 = 10Hz.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Break", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float MonitorInterval = 0.1f;

	// Debug
	// 트레이스/타겟/시야 라인 디버그 드로우
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Debug")
	bool bDebugDraw = true;
};
