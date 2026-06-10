#include "AbilitySystem/Player/BurstWorldActorBase.h"

#include "Components/SceneComponent.h"
#include "Curves/CurveFloat.h"
#include "NiagaraComponent.h"

ABurstWorldActorBase::ABurstWorldActorBase()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
}

void ABurstWorldActorBase::BeginPlay()
{
	Super::BeginPlay();

	// 수명/파괴는 Tick 에서 관리(LifeTime 도달 → 가라앉기 → Destroy).
	// SetLifeSpan 으로 즉시 파괴하지 않는다.

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

	LifeElapsed += DeltaTime;

	// 1) 등장 모션
	if (bMoving)
	{
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

	// 2) 퇴장(가라앉기)
	if (bSinking)
	{
		SinkElapsed += DeltaTime;
		const float RawAlpha = FMath::Clamp(SinkElapsed / SinkDuration, 0.f, 1.f);

		// 가속하며 땅속으로 빨려들어가는 느낌
		const float Alpha = SinkCurve
			? SinkCurve->GetFloatValue(RawAlpha)
			: FMath::InterpEaseIn(0.f, 1.f, RawAlpha, 2.0f);

		const FVector SinkTarget = SinkStartLocation - FVector(0.f, 0.f, SinkDepth);
		SetActorLocation(FMath::Lerp(SinkStartLocation, SinkTarget, Alpha));

		if (RawAlpha >= 1.f)
		{
			Destroy();
		}
		return;
	}

	// 3) 수명 종료 → 가라앉기 시작 (등장 모션이 끝난 뒤)
	if (!bMoving && LifeTime > 0.f && LifeElapsed >= LifeTime)
	{
		BeginSink();
	}
}

void ABurstWorldActorBase::BeginSink()
{
	if (bSinking)
	{
		return;
	}

	if (SinkDuration <= 0.f)
	{
		Destroy();
		return;
	}

	bSinking = true;
	bMoving = false;
	SinkElapsed = 0.f;
	SinkStartLocation = GetActorLocation();

	// 주변 Niagara 화염은 새 파티클 생성을 멈추고 자연스럽게 잦아들게 한다.
	// (Deactivate 는 기존 파티클을 즉시 지우지 않고 각자 수명까지 살려둔다.)
	TArray<UNiagaraComponent*> NiagaraComps;
	GetComponents(NiagaraComps);
	for (UNiagaraComponent* Comp : NiagaraComps)
	{
		if (IsValid(Comp))
		{
			Comp->Deactivate();
		}
	}
}
