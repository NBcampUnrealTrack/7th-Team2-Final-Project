#include "Components/Player/CounterTimeDilationComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"

UCounterTimeDilationComponent::UCounterTimeDilationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Idle 상태에선 틱 불필요. 상태 진입 시 SetState가 켠다.
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

bool UCounterTimeDilationComponent::IsTimeDilationAllowed() const
{
	const UWorld* World = GetWorld();
	return World && World->GetNetMode() == NM_Standalone;
}

void UCounterTimeDilationComponent::StartWindowSlow()
{
	if (!IsTimeDilationAllowed())
	{
		return;
	}

	SetState(ECounterTimeDilationState::WindowSlow);
}

void UCounterTimeDilationComponent::EnterReboost()
{
	if (!IsTimeDilationAllowed())
	{
		return;
	}

	// 카운터 결정은 WindowSlow 구간에서만 유효하다.
	if (State == ECounterTimeDilationState::WindowSlow)
	{
		SetState(ECounterTimeDilationState::Reboost);
	}
}

void UCounterTimeDilationComponent::SetState(ECounterTimeDilationState NewState)
{
	State = NewState;
	StateRealElapsed = 0.f;
	SetComponentTickEnabled(NewState != ECounterTimeDilationState::Idle);

	switch (NewState)
	{
	case ECounterTimeDilationState::WindowSlow:
		// 월드 즉시 슬로우, 플레이어도 함께 느려진다(카운터 결정 구간).
		ApplyGlobalDilation(SlowFactor);
		ApplyPlayerDilation(1.f);
		break;

	case ECounterTimeDilationState::Reboost:
		// 글로벌은 SlowFactor 유지. 플레이어 1.0 → 1/SlowFactor는 Tick에서 보간(시작값 보장).
		ApplyPlayerDilation(1.f);
		break;

	case ECounterTimeDilationState::Rush:
		// 플레이어 정상 속도, 적은 느림.
		ApplyGlobalDilation(SlowFactor);
		ApplyPlayerDilation(1.f / SlowFactor);
		break;

	case ECounterTimeDilationState::Ending:
		// 복구 보간 시작값 스냅샷.
		EndingStartPlayerDilation = GetOwner() ? GetOwner()->CustomTimeDilation : 1.f;
		break;

	case ECounterTimeDilationState::Idle:
		ApplyGlobalDilation(1.f);
		ApplyPlayerDilation(1.f);
		break;
	}
}

void UCounterTimeDilationComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 글로벌 딜레이션의 영향을 받지 않도록 언스케일 실시간 델타로 누적한다.
	StateRealElapsed += static_cast<float>(FApp::GetDeltaTime());

	switch (State)
	{
	case ECounterTimeDilationState::WindowSlow:
		// 카운터 미입력으로 윈도우가 만료되면 복구.
		if (StateRealElapsed >= WindowMaxDuration)
		{
			SetState(ECounterTimeDilationState::Ending);
		}
		break;

	case ECounterTimeDilationState::Reboost:
	{
		const float Dur = FMath::Max(ReboostDuration, KINDA_SMALL_NUMBER);
		const float Alpha = FMath::Clamp(StateRealElapsed / Dur, 0.f, 1.f);
		const float Smooth = Alpha * Alpha * (3.f - 2.f * Alpha); // smoothstep
		ApplyPlayerDilation(FMath::Lerp(1.f, 1.f / SlowFactor, Smooth));
		if (Alpha >= 1.f)
		{
			SetState(ECounterTimeDilationState::Rush);
		}
		break;
	}

	case ECounterTimeDilationState::Rush:
		if (StateRealElapsed >= RushDuration)
		{
			SetState(ECounterTimeDilationState::Ending);
		}
		break;

	case ECounterTimeDilationState::Ending:
	{
		const float Dur = FMath::Max(EndingFade, KINDA_SMALL_NUMBER);
		const float Alpha = FMath::Clamp(StateRealElapsed / Dur, 0.f, 1.f);
		const float Smooth = Alpha * Alpha * (3.f - 2.f * Alpha);
		ApplyGlobalDilation(FMath::Lerp(SlowFactor, 1.f, Smooth));
		ApplyPlayerDilation(FMath::Lerp(EndingStartPlayerDilation, 1.f, Smooth));
		if (Alpha >= 1.f)
		{
			SetState(ECounterTimeDilationState::Idle);
		}
		break;
	}

	default:
		break;
	}
}

void UCounterTimeDilationComponent::ApplyGlobalDilation(float Dilation)
{
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::SetGlobalTimeDilation(World, Dilation);
	}
}

void UCounterTimeDilationComponent::ApplyPlayerDilation(float Dilation)
{
	// 플레이어 실효 속도 = 글로벌 × CustomTimeDilation. 글로벌 슬로우를 상쇄해 정상 속도를 만든다.
	if (AActor* Owner = GetOwner())
	{
		Owner->CustomTimeDilation = Dilation;
	}
}

void UCounterTimeDilationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 레벨 전환/파괴 시 슬로우가 잔류하지 않도록 글로벌 딜레이션을 원복한다(안전장치).
	if (UWorld* World = GetWorld(); IsValid(World) && State != ECounterTimeDilationState::Idle)
	{
		UGameplayStatics::SetGlobalTimeDilation(World, 1.f);
	}

	Super::EndPlay(EndPlayReason);
}