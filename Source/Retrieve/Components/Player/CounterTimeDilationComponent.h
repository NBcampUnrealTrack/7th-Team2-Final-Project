#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CounterTimeDilationComponent.generated.h"

/**
 * 카운터(패리→ParryCounter) 성공 시의 슬로우 연출 상태.
 * 글로벌 타임 딜레이션을 쓰므로 싱글(NM_Standalone) 전용이다. 멀티에선 전체가 no-op.
 */
UENUM()
enum class ECounterTimeDilationState : uint8
{
	Idle,         // 비활성
	WindowSlow,   // 글로벌 슬로우 + 플레이어도 느림. 카운터 결정(Attack) 대기
	Reboost,      // 플레이어만 점진 역보정(가속)
	Rush,         // 플레이어 정상 + 적 느림. 평타 러시
	Ending        // 글로벌·플레이어 정상 복구(ease-out). 미진입 만료도 이 경로
};

/**
 * 카운터 슬로우 연출 전용 컴포넌트(플레이어 부착).
 *
 * 슬로우 생명주기가 패리 성공(GuardAttack) → 카운터(ParryCounter) → 종료로 어빌리티 경계를 넘으므로,
 * 상태를 어빌리티가 아니라 이 컴포넌트가 보유한다. 어빌리티는 StartWindowSlow()/EnterReboost() 신호만 보낸다.
 *
 * 모든 타이밍은 글로벌 딜레이션의 영향을 받지 않도록 실시간(DeltaRealTime) 기준으로 측정한다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class RETRIEVE_API UCounterTimeDilationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCounterTimeDilationComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// 패리 성공(카운터 윈도우 열림) 시 호출. 글로벌 슬로우 시작.
	void StartWindowSlow();

	// 카운터 어택 진입(Attack 입력) 시 호출. 플레이어 역보정 시작(WindowSlow 상태에서만 유효).
	void EnterReboost();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 글로벌 딜레이션은 월드 전체에 영향 → 싱글에서만 허용. 멀티면 연출 전체 비활성.
	bool IsTimeDilationAllowed() const;

	// 상태 전이(진입 시 elapsed 리셋 + Tick on/off). 상태별 딜레이션도 여기서 적용한다.
	void SetState(ECounterTimeDilationState NewState);

	// 글로벌/플레이어 딜레이션 적용 헬퍼.
	void ApplyGlobalDilation(float Dilation);
	void ApplyPlayerDilation(float Dilation);

private:
	// 월드 슬로우 배율. 플레이어 역보정 목표 = 1/SlowFactor.
	UPROPERTY(EditDefaultsOnly, Category = "CounterSlow", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float SlowFactor = 0.2f;

	// WindowSlow 최대 지속(실시간 초). 이 안에 EnterReboost가 안 오면 Ending으로 만료.
	UPROPERTY(EditDefaultsOnly, Category = "CounterSlow", meta = (ClampMin = "0.0"))
	float WindowMaxDuration = 0.8f;

	// 플레이어 역보정(가속) 시간(실시간 초).
	UPROPERTY(EditDefaultsOnly, Category = "CounterSlow", meta = (ClampMin = "0.0"))
	float ReboostDuration = 0.35f;

	// 평타 러시 지속(실시간 초).
	UPROPERTY(EditDefaultsOnly, Category = "CounterSlow", meta = (ClampMin = "0.0"))
	float RushDuration = 1.5f;

	// 복구 ease-out 시간(실시간 초).
	UPROPERTY(EditDefaultsOnly, Category = "CounterSlow", meta = (ClampMin = "0.0"))
	float EndingFade = 0.25f;

	ECounterTimeDilationState State = ECounterTimeDilationState::Idle;

	// 현재 상태에서 경과한 실시간(초). 글로벌 딜레이션 영향을 안 받도록 언스케일 델타를 누적한다.
	float StateRealElapsed = 0.f;

	// Ending 진입 시점의 플레이어 딜레이션(복구 보간 시작값). 러시 후=1/SlowFactor, 미진입 만료=1.0.
	float EndingStartPlayerDilation = 1.f;
};