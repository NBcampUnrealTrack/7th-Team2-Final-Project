#include "AbilitySystem/Player/BurstWorldActorBase.h"

#include "Components/SceneComponent.h"
#include "Curves/CurveFloat.h"

ABurstWorldActorBase::ABurstWorldActorBase()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
}

void ABurstWorldActorBase::BeginPlay()
{
	Super::BeginPlay();

	if (LifeTime > 0.f)
	{
		SetLifeSpan(LifeTime);
	}

	// 스폰된 위치를 목표로, 그 위/아래에서 시작
	TargetLocation = GetActorLocation();
	StartLocation = TargetLocation + FVector(0.f, 0.f, SpawnHeightOffset);

	if (MoveDuration > 0.f && !FMath::IsNearlyZero(SpawnHeightOffset))
	{
		SetActorLocation(StartLocation);
		MoveElapsed = 0.f;
		bMoving = true;
	}
	else
	{
		// 모션 없음 → 즉시 목표 위치
		SetActorLocation(TargetLocation);
		bMoving = false;
	}
}

void ABurstWorldActorBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bMoving)
	{
		return;
	}

	MoveElapsed += DeltaTime;
	const float RawAlpha = FMath::Clamp(MoveElapsed / MoveDuration, 0.f, 1.f);

	// 곡선이 있으면 곡선, 없으면 EaseIn(가속) — 내려찍기/솟구침 둘 다 가속이 자연스러움
	const float Alpha = MoveCurve
		? MoveCurve->GetFloatValue(RawAlpha)
		: FMath::InterpEaseIn(0.f, 1.f, RawAlpha, 2.0f);

	SetActorLocation(FMath::Lerp(StartLocation, TargetLocation, Alpha));

	if (RawAlpha >= 1.f)
	{
		SetActorLocation(TargetLocation);
		bMoving = false;
	}
}
