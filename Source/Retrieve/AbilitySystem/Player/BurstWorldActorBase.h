#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BurstWorldActorBase.generated.h"

class USceneComponent;
class UCurveFloat;
class UNiagaraSystem;

/**
 * 버스트 스킬의 월드 소환 액터 베이스
 *
 * PlayerBurstComponent::DoWorldActorHit 이 SpawnActor 로 생성
 * 데미지 판정은 컴포넌트의 Sweep 이 담당, 이 액터는 비주얼 + 등장 모션만 책임
 * 비주얼(Mesh/Niagara)은 BP 자식에서 SceneRoot 아래로 추가
 *
 */
UCLASS()
class RETRIEVE_API ABurstWorldActorBase : public AActor
{
	GENERATED_BODY()

public:
	ABurstWorldActorBase();

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;

protected:
	/** 루트. 비주얼은 BP 에서 이 아래에 붙인다. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Burst|World", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	/** 자체 수명(초). 0 이하면 무제한. BeginPlay 에서 SetLifeSpan. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Burst|World", meta = (ClampMin = "0.0"))
	float LifeTime = 1.5f;

	/** 등장 시작 높이 오프셋. 양수=위에서 시작(내려찍기), 음수=아래에서 시작(솟구침). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Burst|World|Motion")
	float SpawnHeightOffset = 300.f;

	/** 등장 시작 수평 오프셋(액터 로컬: X=전/후, Y=좌/우). SpawnHeightOffset과 합쳐 대각선 낙하/솟구침.
	 *  예: 뒤·위에서 앞·아래로 대각선 = (X: 음수, Height: 양수). 0이면 수직(기존). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Burst|World|Motion")
	FVector2D SpawnHorizontalOffset = FVector2D::ZeroVector;

	/** 시작 위치 → 목표 위치 도달 시간(초). 0 이하면 즉시 목표 위치. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Burst|World|Motion", meta = (ClampMin = "0.0"))
	float MoveDuration = 0.3f;

	/** 보간 가속 곡선(0~1 정규화 시간 → 0~1 진행도). 없으면 EaseIn(가속) 기본. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Burst|World|Motion")
	TObjectPtr<UCurveFloat> MoveCurve;

	/** 지면 도착(등장 모션 완료) 순간 그 위치에 1회 스폰할 임팩트 이펙트(버섯구름 등). BP 배선 없이 여기 지정. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Burst|World|Impact")
	TSoftObjectPtr<UNiagaraSystem> ImpactVFX;

	/** 퇴장(가라앉기) 길이(초). 0 이하면 가라앉기 없이 즉시 파괴. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Burst|World|Despawn", meta = (ClampMin = "0.0"))
	float SinkDuration = 0.5f;

	/** 가라앉는 깊이(cm). 퇴장 동안 이만큼 아래로 내려간다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Burst|World|Despawn", meta = (ClampMin = "0.0"))
	float SinkDepth = 250.f;

	/** 가라앉기 가속 곡선(0~1 → 0~1). 없으면 EaseIn(가속) 기본. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Burst|World|Despawn")
	TObjectPtr<UCurveFloat> SinkCurve;

private:
	/** 수명 종료 → 가라앉기 단계 진입. 주변 Niagara 를 끄고 하강을 시작한다. */
	void BeginSink();

	/** 지면 도착 시 ImpactVFX 를 TargetLocation 에 1회 스폰. */
	void SpawnImpactVFX();

	/** ImpactVFX 중복 스폰 방지. */
	bool bImpactSpawned = false;

	/** 등장 모션 목표(스폰 시 전달된 위치). */
	FVector TargetLocation = FVector::ZeroVector;

	/** 등장 모션 시작 위치(목표에서 Z 오프셋 적용). */
	FVector StartLocation = FVector::ZeroVector;

	/** 등장 모션 경과 시간. */
	float MoveElapsed = 0.f;

	/** 등장 모션 진행 중 여부. */
	bool bMoving = false;

	/** 전체 생존 경과(초). LifeTime 도달 시 가라앉기 시작. */
	float LifeElapsed = 0.f;

	/** 가라앉기 진행 중 여부. */
	bool bSinking = false;

	/** 가라앉기 경과(초). */
	float SinkElapsed = 0.f;

	/** 가라앉기 시작 위치. */
	FVector SinkStartLocation = FVector::ZeroVector;
};
