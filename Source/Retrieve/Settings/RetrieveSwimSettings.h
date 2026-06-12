#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Settings/AlsMantlingSettings.h"
#include "RetrieveSwimSettings.generated.h"

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

	UPROPERTY(Config, EditAnywhere, Category = "Detection")
	float ChestThreshold = 40.f;
	UPROPERTY(Config, EditAnywhere, Category = "Detection")
	float WadeThreshold = 25.f;
	UPROPERTY(Config, EditAnywhere, Category = "Detection")
	float UnderwaterDepthThreshold = 150.f;
	UPROPERTY(Config, EditAnywhere, Category = "Detection")
	float WadeFloorDistance = 30.f;

	UPROPERTY(Config, EditAnywhere, Category = "Rotation")
	float SwimRotationHalfLife = 0.05f;
	UPROPERTY(Config, EditAnywhere, Category = "Rotation")
	float SwimRotationSpeed = 1000.f;
};
