#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Settings/AlsMantlingSettings.h"
#include "RetrieveSwimSettings.generated.h"

class UMaterialInterface;

/**
 * 수영(MOVE_Flying 기반) 캐릭터 물리/감지/회전 전역 설정.
 * Project Settings > Retrieve > Swim. (per-water 수면태그/offset/flow는 Provider가 담당)
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Retrieve Swim"))
class RETRIEVE_API URetrieveSwimSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URetrieveSwimSettings()
	{
		// 수영 climb-out 전용 트레이스 기본값 (InAir 기본보다 머리맡 가까이 + 더 높은 턱). PIE 튜닝값.
		SwimClimbOutTrace.LedgeHeight = { 50.f, 200.f };
		SwimClimbOutTrace.ReachDistance = 90.f;
	}

	virtual FName GetCategoryName() const override { return TEXT("Retrieve"); }

	UPROPERTY(Config, EditAnywhere, Category = "ClimbOut")
	FAlsMantlingTraceSettings SwimClimbOutTrace;

	UPROPERTY(Config, EditAnywhere, Category = "Speed")
	float MaxSwimSpeed = 300.f;
	UPROPERTY(Config, EditAnywhere, Category = "Speed")
	float SwimSprintMultiplier = 1.5f;

	UPROPERTY(Config, EditAnywhere, Category = "Drag")
	float SwimDrag = 0.5f;
	UPROPERTY(Config, EditAnywhere, Category = "Drag")
	float SwimBraking = 0.f;
	// 수중 보행 항력: 발끝 잠김=1.0배 → 완전 잠수=이 배율(선형). Wade/Submerged-Walk 속도에 적용.
	UPROPERTY(Config, EditAnywhere, Category = "Drag")
	float WadeMinSpeedMultiplier = 0.4f;

	UPROPERTY(Config, EditAnywhere, Category = "Buoyancy")
	float FloatOffset = 40.f;
	UPROPERTY(Config, EditAnywhere, Category = "Buoyancy")
	float BuoyancyStiffness = 4.f;
	UPROPERTY(Config, EditAnywhere, Category = "Buoyancy")
	float BuoyancyDamping = 4.f;
	UPROPERTY(Config, EditAnywhere, Category = "Buoyancy")
	float MaxBuoyancyDepth = 80.f;

	UPROPERTY(Config, EditAnywhere, Category = "Caps")
	float SurfaceSoftBand = 30.f;
	UPROPERTY(Config, EditAnywhere, Category = "Caps")
	float FloorCapDistance = 15.f;
	UPROPERTY(Config, EditAnywhere, Category = "Plunge")
	float PlungeEntrySpeed = 700.f;

	// 발밑 바닥 감지 트레이스 여유거리. 이 안에 바닥 있으면 걷기(Walking), 없으면 수영(Flying).
	UPROPERTY(Config, EditAnywhere, Category = "Detection")
	float WadeFloorDistance = 30.f;

	// 걷기 허용 최대 잠수비율(0~1). 이보다 깊으면 바닥 있어도 수영 유지. 0.5≈허리. 깊은물 바닥 스냅핑 방지.
	UPROPERTY(Config, EditAnywhere, Category = "Detection")
	float WalkMaxSubmersion = 0.5f;

	UPROPERTY(Config, EditAnywhere, Category = "Rotation")
	float SwimRotationHalfLife = 0.05f;
	UPROPERTY(Config, EditAnywhere, Category = "Rotation")
	float SwimRotationSpeed = 1000.f;
	
	// --- Water PP (바이옴 자동선택) ---
	// index = 강 BP의 River Material Presets 값(0=Custom..10=Noise). 기존 
	UPROPERTY(Config, EditAnywhere, Category = "WaterPP")
	TArray<TSoftObjectPtr<UMaterialInterface>> UnderwaterByBiome;
	
	UPROPERTY(Config, EditAnywhere, Category = "WaterPP")
	TSoftObjectPtr<UMaterialInterface> WaterlineMaterial; // 공용 MI_PP_WaterLine_01
};
