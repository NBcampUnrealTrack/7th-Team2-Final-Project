#include "Components/Enemy/EnemySuspicionIndicatorComponent.h"

#include "Components/Enemy/EnemySuspicionIndicatorWidget.h"
#include "Engine/World.h"
#include "TimerManager.h"

UEnemySuspicionIndicatorComponent::UEnemySuspicionIndicatorComponent()
{
	// UWidgetComponent가 Screen space에서 매 프레임 SGameLayerManager에 위젯을 등록·투영해야 하므로 Tick 필수.
	PrimaryComponentTick.bCanEverTick = true;

	SetWidgetSpace(EWidgetSpace::Screen);
	SetDrawSize(FVector2D(64.f, 64.f));
	SetPivot(FVector2D(0.5f, 0.5f));
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetGenerateOverlapEvents(false);

	// WBP 클래스는 아직 없음 — 팀원이 만든 뒤 이 컴포넌트를 상속한 BP 또는
	// 인스턴스 디테일에서 SetWidgetClass로 지정.
}

void UEnemySuspicionIndicatorComponent::SetSuspicionGauge(float InGauge)
{
	const EEnemyAlertUIState PreviousState = GetAlertUIState();
	SuspicionGauge = InGauge;
	const EEnemyAlertUIState NewState = GetAlertUIState();

	// 표시 규칙:
	//   Suspicious: 항상 표시
	//   Suspicious→Alerted 전환: AlertedDisplayDuration만큼 표시 후 숨김 (!가 사라지기 전 최소 유지)
	//   그 외(None, 유지 시간 지난 Alerted, Suspicious 미거친 즉시 Alerted): 숨김
	UWorld* World = GetWorld();
	if (NewState == EEnemyAlertUIState::Suspicious)
	{
		if (World) { World->GetTimerManager().ClearTimer(AlertedHideTimerHandle); }
		SetHiddenInGame(false);
	}
	else if (PreviousState == EEnemyAlertUIState::Suspicious && NewState == EEnemyAlertUIState::Alerted)
	{
		SetHiddenInGame(false);
		if (World && AlertedDisplayDuration > 0.f)
		{
			World->GetTimerManager().SetTimer(AlertedHideTimerHandle, this,
				&UEnemySuspicionIndicatorComponent::HandleAlertedHideTimer, AlertedDisplayDuration, false);
		}
		else
		{
			HandleAlertedHideTimer();
		}
	}
	else if (!AlertedHideTimerHandle.IsValid()
		|| (World && !World->GetTimerManager().IsTimerActive(AlertedHideTimerHandle)))
	{
		// Alerted 표시 타이머가 진행 중이 아니라면 즉시 숨김
		SetHiddenInGame(true);
	}

	if (UEnemySuspicionIndicatorWidget* IndicatorWidget = Cast<UEnemySuspicionIndicatorWidget>(GetUserWidgetObject()))
	{
		if (!bIconsPushed)
		{
			IndicatorWidget->SetIcons(SuspiciousBaseIcon, SuspiciousFillIcon, AlertedIcon);
			bIconsPushed = true;
		}
		IndicatorWidget->UpdateGauge(SuspicionGauge, NewState);
	}
}

void UEnemySuspicionIndicatorComponent::HandleAlertedHideTimer()
{
	SetHiddenInGame(true);
}
